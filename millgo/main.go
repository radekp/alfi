package main

import (
	"fmt"
	"github.com/0xe2-0x9a-0x9b/Go-SDL/sdl"
	"image"
	"image/png"
	"io"
	"io/ioutil"
	"os"
	"strings"
	"unsafe"
)

func abs(value int) int {
	if value >= 0 {
		return value
	}
	return -value
}

func abs32(value int32) int32 {
	if value >= 0 {
		return value
	}
	return -value
}

func min(a, b int32) int32 {
	if a < b {
		return a
	}
	return b
}

func max(a, b int32) int32 {
	if a > b {
		return a
	}
	return b
}

// Trajectory computer and output
type Tco struct {
	x, y, z int32     // current x, y, z
	tx, ty  int32     // target for aggregate moves
	mX, mY  int32     // x, y in machine coordinates
	cmdLen  int       // length of unflushed commands
	cmd     io.Writer // output of commands for arduio driver
}

func flushCmd(t *Tco) {
	if t.cmdLen == 0 {
		return
	}
	fmt.Fprint(t.cmd, "\n")
	t.cmdLen = 0
}

func writeCmd(t *Tco, cmd string) {
	if t.cmdLen+len(cmd) >= 254 {
		flushCmd(t)
	}

	if t.cmdLen == 0 {
		curPosCmd := fmt.Sprintf("x%d y%d z%d c", t.x, t.y, t.z)
		fmt.Fprintf(t.cmd, curPosCmd)
		t.cmdLen = len(curPosCmd)
	}

	if t.cmdLen > 0 {
		fmt.Fprint(t.cmd, " ")
		t.cmdLen++
	}
	fmt.Fprint(t.cmd, cmd)
	t.cmdLen += len(cmd)
}

// Move stepper motor from A to B (in pixel coordinates, 1pixel=0.1mm)
func moveXySimple(t *Tco, x, y int32) (int32, int32) {

	// Stepper motor steps: 5000 steps = 43.6 mm
	//newMx, newMy := (1250 * x) / 109, (1250 * y ) / 109           //  //newMx, newMy := (5000 * bX) / 436, (5000 * bY ) / 436
	newMx, newMy := x, y

	// Compensate x-drift
	//
	// driftX = 24 x-step on 0.5mm
	// driftX = 48 x-steps on 1mm
	//
	// 5000 x-steps = 43.6 mm
	// 48           =  drifxX
	//newMx -= 41856 * t.z / 100000 // approx 0.41856mm on 1mm

	if newMx == t.mX && newMy == t.mY {
		return x, y
	}

	cmd := ""

	if newMx != t.mX {
		cmd += fmt.Sprintf("x%d", newMx)
	}
	if newMy != t.mY {
		if newMx != t.mX {
			cmd += " "
		}
		cmd += fmt.Sprintf("y%d", newMy)
	}
	cmd += " m"

	writeCmd(t, cmd)

	t.x, t.y = x, y
	t.tx, t.ty = x, y
	t.mX, t.mY = newMx, newMy
	return x, y
}

// Aggregate move
func moveXy(t *Tco, x, y int32) (int32, int32) {

	//return moveXySimple(t, x, y)

	x1, y1 := t.tx-t.x, t.ty-t.y
	x2, y2 := x-t.tx, y-t.ty

	if x1*y2 == x2*y1 {
		t.tx, t.ty = x, y
		return x, y
	}

	moveXySimple(t, t.tx, t.ty)
	return moveXySimple(t, x, y)
}

// Move up or down to z

// 5000 steps = 43.6 mm
// 24         = x
//
// driftX = 24 steps
func moveZ(t *Tco, z int32) {

	// Finish pending aggregate moves
	moveXySimple(t, t.tx, t.ty)

	// Always flush pending XY moves so that output is more readable
	flushCmd(t)

	writeCmd(t, "s8000 d4000") // slow speed, motor on z axis is from old printer and must move slowly

	for t.z < z {
		t.z += 5                                 // target 0.5mm down
		moveXySimple(t, t.x, t.y)                // compensate x drift
		writeCmd(t, fmt.Sprintf("z%d m", t.z))   // move 0.5mm down
		writeCmd(t, fmt.Sprintf("z%d m", t.z-2)) // move up & down so that the gear does not slip ;-)
		writeCmd(t, fmt.Sprintf("z%d m", t.z))
	}
	for t.z > z {
		t.z -= 5                               // tartget 0.5mm up
		moveXySimple(t, t.x, t.y)              // compensate x drift
		writeCmd(t, fmt.Sprintf("z%d m", t.z)) // move 0.5mm up
	}

	writeCmd(t, "s4000 d3200") // restore speed
	flushCmd(t)
}

// Used colors. We start with ColMaterial, then we draw the model using
// ColModel. We use ColRemoved for removed material. Material which should
// ideally not be removed but has to be removed because driller radius is
// marked with ColExtraRemoved (see figure below.)
//
//         |          __
//     +---+         /  \
//     |       <-   |   |
//     +---+        \__/
//         |
//
var (
	ColMaterial = uint32(0x000000)
	ColModel    = uint32(0x007f00)
	ColRemoved  = uint32(0x7f0000)
	ColVisited  = uint32(0x800000)
	ColBlue     = uint32(0x0000ff)
	ColGreen    = uint32(0x008000)
	ColDebug    = ColBlue | ColGreen
)

// Directions
var (
	dirN  = int32(0)
	dirS  = int32(1)
	dirE  = int32(2)
	dirW  = int32(3)
	dirNW = int32(4)
	dirNE = int32(5)
	dirSE = int32(6)
	dirSW = int32(7)
)

var (
	dir_N  = int(0)
	dir_S  = int(1)
	dir_E  = int(2)
	dir_W  = int(3)
	dir_NW = int(4)
	dir_NE = int(5)
	dir_SE = int(6)
	dir_SW = int(7)
)

func pngLoad(fname string) (img image.Image, width, height int32) {
	fmt.Printf("loading %s\n", fname)

	file, err := os.Open(fname)
	if err != nil {
		panic(err)
	}
	defer file.Close()

	img, err = png.Decode(file)
	if err != nil {
		panic(err)
	}

	b := img.Bounds()
	width = int32(b.Max.X - b.Min.X)
	height = int32(b.Max.Y - b.Min.Y)

	return img, width, height
}

func pngIsMaterial(x, y int32, img image.Image) bool {
	col := img.At(int(x), int(y))
	r, g, b, a := col.RGBA()
	return r+g+b+a < 32768*3
}

func pngIsEmpty(x, y int32, img image.Image) bool {
	return !pngIsMaterial(x, y, img)
}

func sdlInit(width, height int32) *sdl.Surface {

	var surface = sdl.SetVideoMode(int(width), int(height), 32, sdl.RESIZABLE)

	if surface == nil {
		panic(sdl.GetError())
	}
	return surface
}

func sdlFill(surface *sdl.Surface, w, h int32, color uint32) {
	var x, y int32
	for x = 0; x < w; x++ {
		for y = 0; y < h; y++ {
			sdlSet(x, y, color, surface)
		}
	}
}

func sdlSet(x int32, y int32, value uint32, ss *sdl.Surface) {
	var pix = uintptr(ss.Pixels)
	pix += (uintptr)((y*ss.W)+x) * unsafe.Sizeof(value)
	var pu = unsafe.Pointer(pix)
	var pp *uint32
	pp = (*uint32)(pu)
	*pp |= value
}

func sdlSet2(x int32, y int32, value uint32, ss *sdl.Surface) {
	var pix = uintptr(ss.Pixels)
	pix += (uintptr)((y*ss.W)+x) * unsafe.Sizeof(value)
	var pu = unsafe.Pointer(pix)
	var pp *uint32
	pp = (*uint32)(pu)
	*pp = value
}

func sdlGet(x int32, y int32, ss *sdl.Surface) uint32 {
	var pix = uintptr(ss.Pixels)
	pix += (uintptr)((y*ss.W)+x) * 4
	var pu = unsafe.Pointer(pix)
	var pp *uint32
	pp = (*uint32)(pu)
	return *pp
}

func undrawColor(ss *sdl.Surface, w, h int32, col uint32) {

	var x, y int32
	for y = 0; y < h; y++ {
		for x = 0; x < w; x++ {
			col := sdlGet(x, y, ss) & (^col)
			sdlSet2(x, y, col, ss)
		}
	}
	ss.Flip()
}

func undrawDebug(ss *sdl.Surface, w, h int32) {
	undrawColor(ss, w, h, ColDebug)
}

func undrawBlue(ss *sdl.Surface, w, h int32) {
	undrawColor(ss, w, h, ColBlue)
}

func undrawGreen(ss *sdl.Surface, w, h int32) {
	undrawColor(ss, w, h, ColGreen)
}

// Draw model onto SDL surface
func drawModel(img image.Image, ss *sdl.Surface, offX, offY, w, h int32) {

	var x, y int32
	for y = 0; y < h; y++ {
		for x = 0; x < w; x++ {
			if pngIsMaterial(x, y, img) {
				sdlSet(x+offX, y+offY, ColModel, ss)
			}
		}
	}
	return
}

// Near point iterator
// Iterates points in spiral centered at cx,cy
//
//     9 ....
//       1 2 3
//       8   4
//       7 6 5
//
// If a==0 it will start in cx,cy orherwise a is square side on start
func nearIterBegin(cx, cy, startA int32) (x, y, a int32) {
	return cx - startA, cy - startA, startA
}

// Return next point in spiral
func nearIterNext(cx, cy, prevX, prevY, prevA int32) (x, y, a int32) {

	x, y, a = prevX, prevY, prevA

	if x == cx && y == cy {
		return cx - 1, cy - 1, a
	}

	if y == cy-a {
		x++
		if x-cx <= a {
			return x, y, a
		}
		x, y = cx+a, cy-a+1
		return x, y, a
	}

	if x == cx+a {
		y++
		if y-cy <= a {
			return x, y, a
		}
		x, y = cx+a-1, cy+a
		return x, y, a
	}

	if y == cy+a {
		x--
		if cx-x <= a {
			return x, y, a
		}
		x, y = cx-a, cy+a-1
		return x, y, a
	}

	y--
	if y > cy-a {
		return x, y, a
	}
	a++
	return cx - a, cy - a, a
}

func iterTest() {

	for x, y, a := nearIterBegin(100, 100, 1); a < 100; x, y, a = nearIterNext(100, 100, x, y, a) {
		fmt.Printf("x=%d y=%d a=%d\n", x, y, a)
		fmt.Scanln()
	}
}

func inRect(x, y, w, h int32) bool {
	return x >= 0 && y >= 0 && x < w && y < h
}

// Same as near iter function above just returning points in rectangle
func nearRectBegin(cx, cy, w, h, startA int32) (x, y, a int32, ok bool) {

	x, y, a = nearIterBegin(cx, cy, startA)
	if x >= 0 && y >= 0 && x < w && y < h {
		ok = true
		return
	}
	x, y, a, ok = nearRectNext(cx, cy, x, y, a, w, h)
	return
}

func nearRectNext(cx, cy, prevX, prevY, prevA, w, h int32) (x, y, a int32, ok bool) {

	x, y, a = prevX, prevY, prevA
	for {
		x, y, a = nearIterNext(cx, cy, x, y, a)
		if inRect(x, y, w, h) {
			ok = true
			return
		}
		if a > w+h {
			break
		}
	}
	ok = false
	return
}

// Return true if point x,y is inside of cirlcle with center cx,cy and radius r
func inRadius(x, y, cx, cy, r int32) bool {
	var dx int32 = cx - x
	var dy int32 = cy - y
	return dx*dx+dy*dy <= r*r
}

func inRadiusBegin(cx, cy, r, w, h int32) (x, y int32, ok bool) {
	x, y = cx-r, cy-r
	ok = inRadius(x, y, cx, cy, r) && inRect(x, y, w, h)
	if !ok {
		x, y, ok = inRadiusNext(x, y, cx, cy, r, w, h)
	}
	return
}

// Return next point inside circle with center cx,cy and radius r
func inRadiusNext(x, y, cx, cy, r, w, h int32) (int32, int32, bool) {
	for {
		x++
		if x <= cx+r {
			if inRadius(x, y, cx, cy, r) && inRect(x, y, w, h) {
				return x, y, true
			}
			continue
		}
		y++
		if y <= cy+r {
			x = cx - r
			if inRadius(x, y, cx, cy, r) && inRect(x, y, w, h) {
				return x, y, true
			}
			continue
		}
		break
	}
	return -1, -1, false
}

// Remove material at x,y in radius r. The args w and h are surface dimensions
func removeMaterial(ss *sdl.Surface, rmc [][]int32, w, h, cx, cy, r int32) {

	// Invalidate nearby remove count cache
	for x, y, okR := inRadiusBegin(cx, cy, r*2, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r*2, w, h) {
		if rmc[x][y] > 0 {
			rmc[x][y] = -2
		}
	}

	for x, y, okR := inRadiusBegin(cx, cy, r, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r, w, h) {
		sdlSet(x, y, ColRemoved, ss)
	}
	sdlSet(cx, cy, ColVisited, ss)
	if cx >= w || cx < 0 || cy >= h || cy < 0 {
		panic(fmt.Sprintf("cx=%d cy=%d", cx, cy))
	}
	rmc[cx][cy] = 0
}

// Return volume of material that would be removed at given point. Return -1 if
// model part would be removed
func removeCount(ss *sdl.Surface, rmc [][]int32, w, h, cx, cy, r int32) int32 {

	if !inRect(cx, cy, w, h) {
		return -1
	}

	cached := rmc[cx][cy]
	if cached != -2 {
		return cached
	}

	var count int32 = 0
	for x, y, okR := inRadiusBegin(cx, cy, r, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r, w, h) {
		val := sdlGet(x, y, ss)
		if (val & ColModel) != 0 { // part of model
			rmc[cx][cy] = -1
			return -1
		}
		if (val & ColRemoved) != 0 { // already removed
			continue
		}
		count++
	}

	rmc[cx][cy] = count
	return count
}

// Like removeCount() but add some point to favourize points close to model
func removeCountFavClose(ss *sdl.Surface, rmc [][]int32, w, h, cx, cy, r int32) int32 {

	res := removeCount(ss, rmc, w, h, cx, cy, r)
	if res < 0 {
		return res
	}

	modelNearby := false
	removedNearby := int32(0)

	// Bonus if model is nearby
	for x, y, okR := inRadiusBegin(cx, cy, r+1, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r+1, w, h) {
		if inRadius(x, y, cx, cy, r) {
			continue // skip until just the circle outline
		}
		val := sdlGet(x, y, ss)
		if (val & ColModel) != 0 { // nearby to to model
			modelNearby = true
		}
		if (val & ColRemoved) != 0 {
			removedNearby++
		}
	}

	if modelNearby {
		res *= 1
	}

	//fmt.Printf("res=%d nearby=%d\n", res, removedNearby)
	//7 * (r + 1) - removedNearby

	return (7 * (r + 1) * res) / (removedNearby + 1)
}

// Line from x,y to target tX,tY. Moves until model part is not removed
// or target is reached. Returns new coordinates
func doLine(ss *sdl.Surface, rmc [][]int32, x, y, tX, tY, w, h, r int32, draw, rmMaterial bool) (int32, int32) {

	// Bresenham
	var cx int32 = x
	var cy int32 = y

	var dx int32 = tX - cx
	var dy int32 = tY - cy
	if dx < 0 {
		dx = 0 - dx
	}
	if dy < 0 {
		dy = 0 - dy
	}

	var sx int32
	var sy int32
	if cx < tX {
		sx = 1
	} else {
		sx = -1
	}
	if cy < tY {
		sy = 1
	} else {
		sy = -1
	}
	var err int32 = dx - dy

	for {
		if draw {
			sdlSet(cx, cy, ColDebug, ss)
		} else if removeCount(ss, rmc, w, h, cx, cy, r) < 0 {
			return cx, cy
		}
		if rmMaterial {
			removeMaterial(ss, rmc, w, h, cx, cy, r)
		}

		if (cx == tX) && (cy == tY) {
			break
		}
		var e2 int32 = 2 * err
		if e2 > (0 - dy) {
			err = err - dy
			cx = cx + sx
		}
		if e2 < dx {
			err = err + dx
			cy = cy + sy
		}
	}
	return tX, tY
}

func drawLine(ss *sdl.Surface, rmc [][]int32, x, y, tX, tY int32) {
	doLine(ss, rmc, x, y, tX, tY, -1, -1, 0, true, false)
}

// Is count (in given dir) best?
func bestDir(count, count1, count2, count3, count4, count5, count6, count7 int32) bool {
	return count > 0 &&
		count >= count1 &&
		count >= count2 &&
		count >= count3 &&
		count >= count4 &&
		count >= count5 &&
		count >= count6 &&
		count >= count7
}

var (
	DistMax = int32(0x1fffffff)
)

func dumpDists(d []int32) {
	for i := 0; i < 8; i++ {
		if d[i] != DistMax {
			fmt.Printf("%2d ", d[i])
		} else {
			fmt.Printf("   ")
		}
	}
}

func bestDist(d []int32) (dir int, dist int32) {

	dist = d[0]
	dir = 0
	for i := 1; i < 8; i++ {
		di := d[i]
		if di < dist {
			dist, dir = di, i
		}
	}
	return
}

// Set distance of nearby points (from point ax,ay -> bx,by) in given dir (0=N,
// 1=S, 2=E, 3=W)
func setDist(ss *sdl.Surface, rmc [][]int32, dist [][][]int32, aX, aY, bX, bY, dir, w, h, r, currBestDist, lastSetX, lastSetY int32, done, found bool, round int) (newDone, newFound bool, newBest int32, setX, setY int32) {

	newDone, newFound, newBest, setX, setY = done, found, currBestDist, lastSetX, lastSetY
	
	if !inRect(bX, bY, w, h) {
		return
	}

	dA := dist[aX][aY]
	_, best := bestDist(dA)
	best += 4 * r * r

	if best >= currBestDist { // we alredy have better distance (smaller is better)
		return
	}

	dB := dist[bX][bY]
	rmCount := removeCount(ss, rmc, w, h, bX, bY, r)

	if currBestDist == DistMax && rmCount > 0 && best < DistMax {
		//fmt.Printf("==== found rmCount=%d best=%d bX=%d bY=%d\n", rmCount, best, bX, bY)
		newBest = best
		newFound = true
	}

	//fmt.Printf("setDist aX=%d aY=%d bX=%d bY=%d rmCount=%d\n", aX, aY, bX, bY, rmCount)

	if best < dB[dir] && rmCount != -1 { // part of model would be removed
		if aX&7 == 0 && aY&7 == 0 {
			if round > 20 {
				fmt.Printf("setDist x=%d y=%d value=%d dB[dir]=%d\n", aX, aY, best, dB[dir])
			}
			sdlSet(aX, aY, ColDebug, ss)
			if aX&63 == 0 && aY&63 == 0 {
				ss.Flip()
			}
		}
		dB[dir] = best - rmCount // favourize paths that remove more material
		newDone = false
		setX, setY = bX, bY
	}
	return
}

// Find path from cX,cY to tX,tY so that no part of model is removed
func findAndRemove(ss *sdl.Surface, tc *Tco, rmc [][]int32, cX, cY, w, h, r int32) (bool, bool, int32, int32) {

	//fmt.Printf("findPath cX=%d cY=%d tX=%d tY=%d\n", cX, cY, tX, tY)

	// The algorithm is flood-fill like:
	// For earch pixel we remember shortest distance to target point in 4
	// directions (N,S,E,W). Starting at point (tX,tY) all the distances are 0,
	// e.g. for (tX+1,cY) the distnace in W is 1, others are 3 etc..
	//
	//
	//            S=1     
	//             T  W=1 W=2
	//            N=1     N=3
	//
	// It works like that: N=3 means if you go N the shortest path to T is 3
	// (after going twice west). From the same point it also implies that W=3
	// (after going WN), S=5 (e.g. after going NNWW), E=5 (after WNWW).
	dist := make([][][]int32, w)
	for x := range dist {
		dist[x] = make([][]int32, h)
		for y := range dist[x] {
			if int32(x) == cX && int32(y) == cY {
				dist[x][y] = []int32{0, 0, 0, 0, 0, 0, 0, 0} // value for each dir + removeCount for given point
			} else {
				dist[x][y] = []int32{DistMax, DistMax, DistMax, DistMax, DistMax, DistMax, DistMax, DistMax}
			}
		}
	}

	undrawDebug(ss, w, h)
	currBestDist := DistMax
	found := false
	lastSetX, lastSetY := cX, cY
	for round := 0; ; round++ {

		done := true
		var centerX, centerY int32

		undrawBlue(ss, w, h)
		//drawLine(ss, rmc, cX, cY, tX, tY)

		if round%8 == 0 {
			centerX, centerY = cX, cY
		} else if round%8 == 0 {
			centerX, centerY = 0, 0
		} else if round%8 == 1 {
			centerX, centerY = 0, w-1
		} else if round%8 == 2 {
			centerX, centerY = 0, h-1
		} else if round%8 == 3 {
			centerX, centerY = w-1, h-1
		} else {
            centerX, centerY = lastSetX, lastSetY
        }

		for x, y, a, ok := nearRectBegin(centerX, centerY, w, h, 0); ok; x, y, a, ok = nearRectNext(centerX, centerY, x, y, a, w, h) {

			/*fmt.Printf(" N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW\n")
			for i := cY - 3; i <= cY+3; i++ {
				for j := cX - 3; j <= cX+3; j++ {
					if !inRect(i, j, w, h) {
						continue
					}
					dumpDists(dist[j][i])
					fmt.Printf("| ")
				}
				fmt.Println()
			}
			fmt.Printf("x=%d y=%d a=%d done=%t currBestDist=%d\n", x, y, a, done, currBestDist)
			fmt.Scanln()*/

			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x, y+1, dirN, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x, y-1, dirS, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x-1, y, dirE, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x+1, y, dirW, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x+1, y+1, dirNW, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x-1, y-1, dirSE, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x-1, y+1, dirNE, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
			done, found, currBestDist, lastSetX, lastSetY = setDist(ss, rmc, dist, x, y, x+1, y-1, dirSW, w, h, r, currBestDist, lastSetX, lastSetY, done, found, round)
		}

		fmt.Printf("round=%d done=%t found=%t currBestDist=%d, lastSetX=%d lastSetY=%d\n", round, done, found, currBestDist, lastSetX, lastSetY)

		if done {
			break
		}
	}

	dstMin := DistMax
	tX, tY := int32(-1), int32(-1)
	removed := false
	for x, y, a, ok := nearRectBegin(cX, cY, w, h, 1); ok; x, y, a, ok = nearRectNext(cX, cY, x, y, a, w, h) {

		if removeCount(ss, rmc, w, h, x, y, r) <= 0 {
			continue
		}
		removed = true
		dst := DistMax
		if found {
			_, dst = bestDist(dist[x][y])
		} else {
			dst = abs32(cX-x) + abs32(cY-y)
		}
		if dst < dstMin {
			dstMin, tX, tY = dst, x, y
		}
	}

	fmt.Printf("removed=%t found=%t dstMin=%d tX=%d tY=%d\n", removed, found, dstMin, tX, tY)

	if !removed {
		return false, false, tX, tY
	}
	if !found {
		return true, false, tX, tY
	}

	var dirs []int

	for x, y := tX, tY; x != cX || y != cY; {

		dir, _ := bestDist(dist[x][y])

		dirs = append(dirs, dir)
		if dir == dir_N {
			y--
		} else if dir == dir_S {
			y++
		} else if dir == dir_E {
			x++
		} else if dir == dir_W {
			x--
		} else if dir == dir_NW {
			x, y = x-1, y-1
		} else if dir == dir_NE {
			x, y = x+1, y-1
		} else if dir == dir_SW {
			x, y = x-1, y+1
		} else if dir == dir_SE {
			x, y = x+1, y+1
		}

		/*fmt.Printf(" N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW|  N  S  E  W  NW NE SE SW\n")
				for i := y - 3; i <= y+3; i++ {
					for j := x - 3; j <= x+3; j++ {
						if !inRect(i, j, w, h) {
							continue
						}
						dumpDists(dist[j][i])
						fmt.Printf("| ")
					}
					fmt.Println()
				}
				fmt.Printf("x=%d y=%d dir=%d bestDist=%d\n", x, y, dir, dst)
		        fmt.Scanln()*/
	}

	for x, y, i := cX, cY, len(dirs)-1; i >= 0; i-- {
		dir := dirs[i]
		if dir == dir_N {
			y++
		} else if dir == dir_S {
			y--
		} else if dir == dir_E {
			x--
		} else if dir == dir_W {
			x++
		} else if dir == dir_NW {
			x, y = x+1, y+1
		} else if dir == dir_NE {
			x, y = x-1, y+1
		} else if dir == dir_SW {
			x, y = x+1, y-1
		} else if dir == dir_SE {
			x, y = x-1, y-1
		}
		//fmt.Printf("cX=%d cY=%d tX=%d tY=%d x=%d y=%d\n", cX, cY, tX, tY, x, y)
		moveXy(tc, x, y)
		removeMaterial(ss, rmc, w, h, x, y, r)
		sdlSet(x, y, ColDebug, ss)
		ss.Flip()
		//fmt.Scanln()
	}

	return true, true, tX, tY
}

func makeRmc(w, h int32) [][]int32 {
	rmc := make([][]int32, w)
	for x := range rmc {
		rmc[x] = make([]int32, h)
		for y := range rmc[x] {
			rmc[x][y] = -2 // -2 means not computed yet or invalidated
		}
	}
	return rmc
}

// Compute milling trajectory, before exit the position is always returned to
// point 0,0
func computeTrajectory(pngFile string, tc *Tco, z, r int32) {

	img, w, h := pngLoad(pngFile)       // image
	ss := sdlInit(w+2*(r+1), h+2*(r+1)) // sdl surface - with radius+1 border
	sdlFill(ss, w, h, ColMaterial)      // we have all material in the begining then we remove the parts so that just model is left
	drawModel(img, ss, r+1, r+1, w, h)  // draw the model with green so that we see if we are removing correct parts
	w, h = w+2*(r+1), h+2*(r+1)

	// Cache for removeCount()
	rmc := makeRmc(w, h)

	var x, y int32 = 0, 0

	moveZ(tc, z)

	// Start searching point to remove in where is material - this will fastly
	// remove most of the material. Then we switch to visited mode - this will
	// search in find2Remove also in removed material and will remove the small
	// remaining parts
	for {

		found, removed, tX, tY := findAndRemove(ss, tc, rmc, x, y, w, h, r)

		fmt.Printf("findAndRemove x=%d y=%d tX=%d tY=%d found=%t removed=%t\n",
			x, y, tX, tY, found, removed)

		if !found {
			break // we are done on this level
		}

		if !removed {
			fmt.Printf("move up, to %d,%d and down to continue\n", tX, tY)
			moveZ(tc, 0)
			x, y = moveXy(tc, tX, tY)
			moveZ(tc, z)
			//fmt.Scanln()
		}

		x, y = tX, tY

		removeMaterial(ss, rmc, w, h, x, y, r)
		ss.Flip()

		for {
			countN := removeCountFavClose(ss, rmc, w, h, x, y-1, r)
			countS := removeCountFavClose(ss, rmc, w, h, x, y+1, r)
			countE := removeCountFavClose(ss, rmc, w, h, x+1, y, r)
			countW := removeCountFavClose(ss, rmc, w, h, x-1, y, r)

			countNE := removeCountFavClose(ss, rmc, w, h, x+1, y-1, r)
			countSE := removeCountFavClose(ss, rmc, w, h, x+1, y+1, r)
			countSW := removeCountFavClose(ss, rmc, w, h, x-1, y+1, r)
			countNW := removeCountFavClose(ss, rmc, w, h, x-1, y-1, r)

			countN *= 2
			countS *= 2

			//fmt.Printf("== x=%d y=%d\n", x, y)

			if bestDir(countN, countS, countE, countW, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y-1)
			} else if bestDir(countS, countN, countE, countW, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y+1)
			} else if bestDir(countE, countN, countS, countW, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x+1, y)
			} else if bestDir(countW, countN, countE, countS, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x-1, y)
			} else if bestDir(countNE, countN, countS, countE, countW, countSE, countSW, countNW) {
				x, y = moveXy(tc, x+1, y-1)
			} else if bestDir(countSE, countN, countS, countE, countW, countNE, countSW, countNW) {
				x, y = moveXy(tc, x+1, y+1)
			} else if bestDir(countSW, countN, countS, countE, countW, countNE, countSE, countNW) {
				x, y = moveXy(tc, x-1, y+1)
			} else if bestDir(countNW, countN, countS, countE, countW, countNE, countSE, countSW) {
				x, y = moveXy(tc, x-1, y-1)
			} else {
				//fmt.Printf("no good dir %d %d %d %d %d %d %d %d\n", countN, countS, countE, countW, countNE, countSE, countSW, countNW)
				break
			}

			removeMaterial(ss, rmc, w, h, x, y, r)
			ss.Flip()
		}
	}

	moveZ(tc, 0)
	x, y = moveXy(tc, 0, 0)

	fmt.Printf("done at z=%d!\n", z)
}

func drawTrajectory(txtFile string, r int32) {

	file, err := os.Open(txtFile)
	if err != nil {
		panic(err)
	}
	defer file.Close()

	w, h := int32(1280), int32(1024)
	ss := sdlInit(w, h)            // sdl surface - with radius+1 border
	sdlFill(ss, w, h, ColMaterial) // we have all material in the begining then we remove the parts so that just model is left

	// Cache for removeCount()
	rmc := makeRmc(w, h)

	content, err := ioutil.ReadFile(txtFile)
	if err != nil {
		panic(err)
	}
	lines := strings.Split(string(content), "\n")

	trajLen := int32(0)
	x, y, z := int32(0), int32(0), int32(0)
	for i := 0; i < len(lines); i++ {
		line := lines[i] + "\n"
		//fmt.Printf("%d) %s\n", i, line)
		var arg int32 = 0
		var cmd byte = '_'
		tx, ty, tz := x, y, z

		for j := 0; j < len(line); j++ {
			var c uint8 = line[j]
			switch {
			case '0' <= c && c <= '9':
				arg = arg*10 + int32(c-'0')
			default:
				arg = 0
				cmd = c
			case c == ' ' || c == '\n':
				//fmt.Printf("cmd=%c arg=%d\n", cmd, arg)
				//fmt.Scanln()
				switch cmd {
				case 'x':
					tx = arg
				case 'y':
					ty = arg
				case 'z':
					tz = arg
				case 'm':
					//tX, tY := x + (109 * (tar[0] - pos[0])) / 1250, y + (109 * (tar[1] - pos[1])) / 1250

					//fmt.Printf("tX=%d tY=%d\n", tX, tY)
					//fmt.Scanln()

					if inRect(x, y, w, h) && inRect(tx, ty, w, h) {
						drawLine(ss, rmc, x, y, tx, ty)
						doLine(ss, rmc, x, y, tx, ty, w, h, r, false, true)
						ss.Flip()
						trajLen += max(abs32(x-tx), abs32(y-ty))
					}
					x, y, z = tx, ty, tz
				case 'c':
					x, y, z = tx, ty, tz
				}
			}
		}
	}
	for {
		fmt.Printf("done! trajectory len=%dmm\n", trajLen/10)
		fmt.Scanln()
		ss.Flip()
	}
}

func usage() {
	fmt.Printf("usage: %s shape0mm.png shape1mm.png (to compute trajectory)\n       %s shape0mm.txt shape1mm.txt (to draw trajectory)\n",
		os.Args[0], os.Args[0])
}

func main() {

	if len(os.Args) <= 1 {
		usage()
		return
	}

	var zStep = 10   // z step is 1mm
	var r int32 = 18 // the case is designed to be milled with 4mm driller, but i have just 3.6mm
	tco := Tco{0, 0, 0, 0, 0, 0, 0, 0, os.Stderr}
	tc := &tco

	for i := 1; i < len(os.Args); i++ {

		filename := os.Args[i]
		if strings.HasSuffix(filename, ".png") {
			computeTrajectory(filename, tc, int32(i*zStep), r)
		} else if strings.HasSuffix(filename, ".txt") {
			drawTrajectory(filename, r)
		} else {
			usage()
		}
	}
}
