package main

import (
	"fmt"
	"github.com/0xe2-0x9a-0x9b/Go-SDL/sdl"
	"image"
	"image/png"
	"io"
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
	pX, pY int32     // current pixel coordinates
	mX, mY int32     // current machine coordinates
	z      int32     // height
	cmdLen int       // length of unflushed commands
	cmd    io.Writer // output of commands for arduio driver
}

// Move stepper motor from A to B (in pixel coordinates, 1pixel=0.1mm)
func moveXySimple(t *Tco, aX, aY, bX, bY int32) (int32, int32) {

	if aX != t.pX || aY != t.pY {
		panic(fmt.Sprintf("unexpected move t.pX=%d t.pY=%d aX=%d aY=%d", t.pX, t.pY, aX, aY))
	}

	// Stepper motor steps: 5000 steps = 43.6 mm
	newMx, newMy := (1250 * bX) / 109, (1250 * bY ) / 109           //  //newMx, newMy := (5000 * bX) / 436, (5000 * bY ) / 436

	if newMx == t.mX && newMy == t.mY {
		return bX, bY
	}

	cmd := ""

	if newMx != t.mX {
		cmd += fmt.Sprintf("a0 p%d t%d", t.mX, newMx)
	}
	if newMy != t.mY {
		if newMx != t.mX {
			cmd += " "
		}
		cmd += fmt.Sprintf("a1 p%d t%d", t.mY, newMy)
	}
	cmd += " m"

	if t.cmdLen+len(cmd) >= 254 {
		fmt.Fprint(t.cmd, "\n")
		t.cmdLen = 0
	} else {
		if t.cmdLen > 0 {
			fmt.Fprint(t.cmd, " ")
			t.cmdLen++
		}
	}
	fmt.Fprint(t.cmd, cmd)
	t.cmdLen += len(cmd)

	t.pX, t.pY = bX, bY
	t.mX, t.mY = newMx, newMy

	return bX, bY
}

func moveXy(t *Tco, aX, aY, bX, bY int32) (int32, int32) {

	// Aggregate move
	x1, y1 := aX-t.pX, aY-t.pY
	x2, y2 := bX-aX, bY-aY
	//fmt.Printf("x1=%d y2=%d   x2=%d y1=%d\n", x1, y2, x2, y1)    
	if x1*y2 == x2*y1 {
		return bX, bY
	}

	moveXySimple(t, t.pX, t.pY, aX, aY)
	return moveXySimple(t, aX, aY, bX, bY)
}

// Move z up or down in 0.5mm steps
func moveZ(t *Tco, z int32) {
    
    // Always flush XY moves
    if(t.cmdLen > 0) {
        fmt.Fprint(t.cmd, "\n")
        t.cmdLen = 0
    }
    
    fmt.Fprintf(t.cmd, "s8000 d4000\n")     // slow speed, motor on z axis is from old printer and must move slowly
    
    for z > 0 {
        
        fmt.Fprintf(t.cmd, "a2 p0 t437 m\n")       // move 0.5mm down
        fmt.Fprintf(t.cmd, "a0 p0 t24 m\n")        // compensate x drift

        fmt.Fprintf(t.cmd, "a2 p437 t309 m\n")     // move up & down so that the gear does not slip ;-)
        fmt.Fprintf(t.cmd, "a2 p309 t437 m\n")

        //newDriftX += 24;
        z--;
    }
    for z < 0 {
        fmt.Fprintf(t.cmd, "a0 p24 t0 m\n")       // compensate x drift and move up
        fmt.Fprintf(t.cmd, "a2 p437 t0 m\n")      // move 0.5mm up
        
        //newDriftX -= 24;
        z++;
    }

    fmt.Fprintf(t.cmd, "s4000 d3000\n");    // restore speed
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
func removeMaterial(ss *sdl.Surface, w, h, cx, cy, r int32) {

	for x, y, okR := inRadiusBegin(cx, cy, r, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r, w, h) {
		sdlSet(x, y, ColRemoved, ss)
	}
	sdlSet(cx, cy, ColVisited, ss)
}

// Return volume of material that would be removed at given point. Return -1 if
// model part would be removed
func removeCount(ss *sdl.Surface, w, h, cx, cy, r int32) int32 {

	if !inRect(cx, cy, w, h) {
		return -1
	}

	var count int32 = 0
	for x, y, okR := inRadiusBegin(cx, cy, r, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r, w, h) {
		val := sdlGet(x, y, ss)
		if (val & ColModel) != 0 { // part of model
			return -1
		}
		if (val & ColRemoved) != 0 { // already removed
			continue
		}
		count++
	}
	return count
}

// Like removeCount() but add some point to favourize points close to model
func removeCountFavClose(ss *sdl.Surface, w, h, cx, cy, r int32) int32 {

	res := removeCount(ss, w, h, cx, cy, r)
	if res < 0 {
		return res
	}

	for x, y, okR := inRadiusBegin(cx, cy, r+1, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r+1, w, h) {
		if inRadius(x, y, cx, cy, r) {
			continue // skip until just the circle outline
		}
		val := sdlGet(x, y, ss)
		if (val & ColModel) != 0 { // nearby to to model
			res *= 2
		}
	}

	return res
}

// Line from x,y to target tX,tY. Moves until model part is not removed
// or target is reached. Returns new coordinates
func doLine(ss *sdl.Surface, x, y, tX, tY, w, h, r int32, draw, rmMaterial bool) (int32, int32) {

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
		} else if removeCount(ss, w, h, cx, cy, r) < 0 {
			return cx, cy
		}
		if rmMaterial {
			removeMaterial(ss, w, h, cx, cy, r)
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

func drawLine(ss *sdl.Surface, x, y, tX, tY int32) {
	doLine(ss, x, y, tX, tY, -1, -1, 0, true, false)
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
	for i := 0; i < 4; i++ {
		if d[i] != DistMax {
			fmt.Printf("%2d ", d[0])
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
func setDist(ss *sdl.Surface, dist [][][]int32, aX, aY, bX, bY, dir, w, h, r, currBestDist int32, done bool) bool {

	if !inRect(bX, bY, w, h) {
		return done
	}

	dA := dist[aX][aY]
	_, best := bestDist(dA)
	best += 4 * r * r

	if best >= currBestDist { // we alredy have better distance (smaller is better)
		return done
	}

	dB := dist[bX][bY]
	rmCount := dB[8]
	if rmCount == -2 {
		rmCount = removeCount(ss, w, h, bX, bY, r)
		dB[8] = rmCount
	}

	if best < dB[dir] && rmCount != -1 { // part of model would be removed
		if aX%4 == 0 && aY%4 == 0 {
			// fmt.Printf("setDist x=%d y=%d value=%d currBestDist=%d\n", aX, aY, best, currBestDist)
			sdlSet(aX, aY, ColDebug, ss)
			ss.Flip()
		}
		dB[dir] = best - rmCount // favourize paths that remove more material
		return false
	}
	return done
}

// Find path from cX,cY to tX,tY so that no part of model is removed
func findPath(ss *sdl.Surface, tc *Tco, cX, cY, tX, tY, w, h, r int32) bool {

	//fmt.Printf("findPath cX=%d cY=%d tX=%d tY=%d removeCount=%d\n", cX, cY, tX, tY, removeCount(ss, w, h, tX, tY, r))

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
	tx, ty := int(tX), int(tY)
	dist := make([][][]int32, w)
	for x := range dist {
		dist[x] = make([][]int32, h)
		for y := range dist[x] {
			if x == tx && y == ty {
				dist[x][y] = []int32{0, 0, 0, 0, 0, 0, 0, 0, -2} // value for each dir + removeCount for given point
			} else {
				dist[x][y] = []int32{DistMax, DistMax, DistMax, DistMax, DistMax, DistMax, DistMax, DistMax, -2}
			}
		}
	}

	rr4 := 4 * r * r

	undrawDebug(ss, w, h)
	currBestDist := DistMax
	for round := 0; ; round++ {

		var done bool
		var centerX, centerY int32

		undrawBlue(ss, w, h)
		drawLine(ss, cX, cY, tX, tY)

		if round%5 == 0 {
			done = true
			centerX, centerY = tX, tY
			centerY = tY
		} else if round%5 == 1 {
			centerX, centerY = 0, 0
		} else if round%5 == 2 {
			centerX, centerY = 0, w-1
		} else if round%5 == 3 {
			centerX, centerY = 0, h-1
		} else if round%5 == 4 {
			centerX, centerY = w-1, h-1
		}

		for x, y, a, ok := nearRectBegin(centerX, centerY, w, h, 0); ok; x, y, a, ok = nearRectNext(centerX, centerY, x, y, a, w, h) {

			_, currBestDist = bestDist(dist[cX][cY])
			if rr4*abs32(x-cX) >= currBestDist || rr4*abs32(y-cY) >= currBestDist {
				continue
			}

			/*for i := tY - 3; i <= tY+3; i++ {
				for j := tX - 3; j <= tX+3; j++ {
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

			done = setDist(ss, dist, x, y, x, y+1, dirN, w, h, r, currBestDist, done)
			done = setDist(ss, dist, x, y, x, y-1, dirS, w, h, r, currBestDist, done)
			done = setDist(ss, dist, x, y, x-1, y, dirE, w, h, r, currBestDist, done)
			done = setDist(ss, dist, x, y, x+1, y, dirW, w, h, r, currBestDist, done)
			//done = setDist(ss, dist, x, y, x+1, y+1, dirSE, w, h, r, currBestDist, done)
			//done = setDist(ss, dist, x, y, x-1, y-1, dirNW, w, h, r, currBestDist, done)
			//done = setDist(ss, dist, x, y, x-1, y+1, dirSW, w, h, r, currBestDist, done)
			//done = setDist(ss, dist, x, y, x+1, y-1, dirNE, w, h, r, currBestDist, done)
		}

		//if currBestDist < DistMax {
		//break
		//}

		if done {
			if currBestDist == DistMax {
				return false
			}
			break
		}
	}

	for x, y := cX, cY; x != tX || y != tY; {

		dir, _ := bestDist(dist[x][y])
		if dir == dir_N {
			x, y = moveXy(tc, x, y, x, y-1)
		} else if dir == dir_S {
			x, y = moveXy(tc, x, y, x, y+1)
		} else if dir == dir_E {
			x, y = moveXy(tc, x, y, x+1, y)
		} else if dir == dir_W {
			x, y = moveXy(tc, x, y, x-1, y)
		} else if dir == dir_NW {
			x, y = moveXy(tc, x, y, x-1, y-1)
		} else if dir == dir_NE {
			x, y = moveXy(tc, x, y, x+1, y-1)
		} else if dir == dir_SW {
			x, y = moveXy(tc, x, y, x-1, y+1)
		} else if dir == dir_SE {
			x, y = moveXy(tc, x, y, x+1, y+1)
		}

		//fmt.Printf("x=%d y=%d dir=%d\n", x, y, dir)
		removeMaterial(ss, w, h, x, y, r)
		sdlSet(x, y, ColDebug, ss)
		ss.Flip()
	}

	return true
}

// Same as find2Remove but tries to find point on direct line from curr point
func findAndRemove(ss *sdl.Surface, tc *Tco, currX, currY, w, h, r int32) (bool, bool, int32, int32) {

	undrawDebug(ss, w, h)
	found := false
	firstTx, firstTy := currX, currY

	for tX, tY, a, ok := nearRectBegin(currX, currY, w, h, 1); ok; tX, tY, a, ok = nearRectNext(currX, currY, tX, tY, a, w, h) {

		if removeCount(ss, w, h, tX, tY, r) <= 0 {
			continue
		}
		if !found {
			found = true
			firstTx, firstTy = tX, tY
		}

		// Straight line
		x, y := doLine(ss, currX, currY, tX, tY, w, h, r, false, false)
		if x == tX && y == tY {
			doLine(ss, currX, currY, tX, tY, w, h, r, false, true)
			moveXy(tc, currX, currY, tX, tY)
			return true, true, tX, tY
		}
		if tX%4 == 0 && tY%4 == 0 {
			sdlSet(tX, tY, ColDebug, ss)
			drawLine(ss, currX, currY, tX, tY)
			ss.Flip()
		}
	}
	//fmt.Printf("findAndRemove - no straight line\n")

	if found {
		if findPath(ss, tc, currX, currY, firstTx, firstTy, w, h, r) {
			return true, true, firstTx, firstTy
		}
	}

	return found, false, firstTx, firstTy
}

// Compute milling trajectory, before exit the position is always returned to
// point 0,0
func computeTrajectory(pngFile string, tc *Tco, z, r int32) {

	img, w, h := pngLoad(pngFile)       // image
	ss := sdlInit(w+2*(r+1), h+2*(r+1)) // sdl surface - with radius+1 border
	sdlFill(ss, w, h, ColMaterial)      // we have all material in the begining then we remove the parts so that just model is left
	drawModel(img, ss, r+1, r+1, w, h)  // draw the model with green so that we see if we are removing correct parts
	w, h = w+2*(r+1), h+2*(r+1)

	var x, y int32 = 0, 0

	moveZ(tc, z)
	
	// Start searching point to remove in where is material - this will fastly
	// remove most of the material. Then we switch to visited mode - this will
	// search in find2Remove also in removed material and will remove the small
	// remaining parts
	for {

		found, removed, tX, tY := findAndRemove(ss, tc, x, y, w, h, r)

		//fmt.Printf("findAndRemove x=%d y=%d tX=%d tY=%d found=%t removed=%t\n",
		//         x, y, tX, tY, found, removed)

		if !found {
			break // we are done on this level
		}

		if !removed {
            fmt.Printf("move up, to %d,%d and down to continue\n", tX, tY)
            moveZ(tc, -z)
            x, y = moveXy(tc, x, y, tX, tY)
            moveZ(tc, z)
			//fmt.Scanln()
		}

		x, y = tX, tY

		removeMaterial(ss, w, h, x, y, r)
		ss.Flip()

		for {
			countN := removeCountFavClose(ss, w, h, x, y-1, r)
			countS := removeCountFavClose(ss, w, h, x, y+1, r)
			countE := removeCountFavClose(ss, w, h, x+1, y, r)
			countW := removeCountFavClose(ss, w, h, x-1, y, r)

			countNE := removeCountFavClose(ss, w, h, x+1, y-1, r)
			countSE := removeCountFavClose(ss, w, h, x+1, y+1, r)
			countSW := removeCountFavClose(ss, w, h, x-1, y+1, r)
			countNW := removeCountFavClose(ss, w, h, x-1, y-1, r)

			//fmt.Printf("== x=%d y=%d\n", x, y)

			if bestDir(countN, countS, countE, countW, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y, x, y-1)
			} else if bestDir(countS, countN, countE, countW, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y, x, y+1)
			} else if bestDir(countE, countN, countS, countW, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y, x+1, y)
			} else if bestDir(countW, countN, countE, countS, countNE, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y, x-1, y)
			} else if bestDir(countNE, countN, countS, countE, countW, countSE, countSW, countNW) {
				x, y = moveXy(tc, x, y, x+1, y-1)
			} else if bestDir(countSE, countN, countS, countE, countW, countNE, countSW, countNW) {
				x, y = moveXy(tc, x, y, x+1, y+1)
			} else if bestDir(countSW, countN, countS, countE, countW, countNE, countSE, countNW) {
				x, y = moveXy(tc, x, y, x-1, y+1)
			} else if bestDir(countNW, countN, countS, countE, countW, countNE, countSE, countSW) {
				x, y = moveXy(tc, x, y, x-1, y-1)
			} else {
				//fmt.Printf("no good dir %d %d %d %d %d %d %d %d\n", countN, countS, countE, countW, countNE, countSE, countSW, countNW)
				break
			}

			removeMaterial(ss, w, h, x, y, r)
			ss.Flip()
		}
	}

    moveZ(tc, -z)
    x, y = moveXy(tc, x, y, 0, 0)
    
    fmt.Printf("done!\n")
}

func drawTrajectory(txtFile string) {
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

    var r int32 = 16 // the case is designed to be milled with 4mm driller, but i have just 3.2mm
    tco := Tco{0, 0, 0, 0, 0, 0, os.Stderr}
    tc := &tco

	for i := 1; i < len(os.Args); i++ {

		filename := os.Args[i]
		if strings.HasSuffix(filename, ".png") {
			computeTrajectory(filename, tc, int32(i), r)
		} else if strings.HasSuffix(filename, ".txt") {
			drawTrajectory(filename)
		} else {
			usage()
		}
	}
}
