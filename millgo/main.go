package main

import (
	"fmt"
	"github.com/0xe2-0x9a-0x9b/Go-SDL/sdl"
	"os"
	"unsafe"
	//"math/rand"
	"image"
	"image/png"
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
	ColModel    = uint32(0x00ff00)
	ColRemoved  = uint32(0x7f0000)
	ColVisited  = uint32(0x800000)
	ColDebug    = uint32(0x0000ff)
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

func undrawDebug(ss *sdl.Surface, w, h int32) {
    
    var x, y int32
    for y = 0; y < h; y++ {
        for x = 0; x < w; x++ {
            col := sdlGet(x, y, ss) & (^ColDebug)
            sdlSet2(x, y, col, ss)
        }
    }
    ss.Flip()
}

// Draw model onto SDL surface
func drawModel(img image.Image, ss *sdl.Surface, w, h int32) {

	var x, y int32
	for y = 0; y < h; y++ {
		for x = 0; x < w; x++ {
			if pngIsMaterial(x, y, img) {
				sdlSet(x, y, ColModel, ss)
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
func nearIterBegin(cx, cy int32) (x, y, a int32) {
	x, y, a = cx-1, cy-1, 1
	return
}

// Return next point in spiral
func nearIterNext(cx, cy, prevX, prevY, prevA int32) (x, y, a int32) {

	x, y, a = prevX, prevY, prevA

	if y == cy-a {
		x++
		if x-cx <= a {
			return x, y, a
		}
		x = cx + a
		y = cy - a + 1
		return x, y, a
	}

	if x == cx+a {
		y++
		if y-cy <= a {
			return x, y, a
		}
		x = cx + a - 1
		y = cy + a
		return x, y, a
	}

	if y == cy+a {
		x--
		if cx-x <= a {
			return x, y, a
		}
		x = cx - a
		y = cy + a - 1
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

	for x, y, a := nearIterBegin(100, 100); a < 100; x, y, a = nearIterNext(100, 100, x, y, a) {
		fmt.Printf("x=%d y=%d a=%d\n", x, y, a)
		fmt.Scanln()
	}
}

func inRect(x, y, w, h int32) bool {
	return x >= 0 && y >= 0 && x < w && y < h
}

// Same as near iter function above just returning points in rectangle
func nearRectBegin(cx, cy, w, h int32) (x, y, a int32, ok bool) {

	x, y, a = nearIterBegin(cx, cy)
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

// Find a x, y where is material to be removed. Returns closes point, but
// skipCount can be used to return n-th best.
func find2Remove(ss *sdl.Surface, currX, currY, w, h, r, skipCount int32, skipCol uint32) (bool, int32, int32) {

	for cx, cy, a, ok := nearRectBegin(currX, currY, w, h); ok; cx, cy, a, ok = nearRectNext(currX, currY, cx, cy, a, w, h) {

		val := sdlGet(cx, cy, ss)
		//fmt.Printf("x=%d y=%d val=%d\n", x, y, val)
		if (val & skipCol) != 0 { // already visited/removed material
			continue
		}

		// Check if we dont remove part of model in range
		remModel := (val & ColModel) != 0
		remMaterial := (val == 0)

		for x, y, okR := inRadiusBegin(cx, cy, r, w, h); okR; x, y, okR = inRadiusNext(x, y, cx, cy, r, w, h) {

			val := sdlGet(x, y, ss)
			remModel = remModel || ((val & ColModel) != 0)
			remMaterial = remMaterial || (val == 0)
		}

		if remMaterial && !remModel {
			if skipCount <= 0 {
				return true, cx, cy
			}
			skipCount--
		}
	}

	return false, -1, -1
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

// Linear path from x,y to target tX,tY. Moves until model part is not removed
// or target is reached. Returns new coordinates
func moveOnLine(ss *sdl.Surface, x, y, tX, tY, w, h, r int32) (int32, int32) {

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
		if removeCount(ss, w, h, cx, cy, r) < 0 {
			return cx, cy
		}
		removeMaterial(ss, w, h, cx, cy, r)

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
		sdlSet(cx, cy, ColDebug, ss)
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
}

/*
func checkPath(ss *sdl.Surface, x, y, tX, tY, w, h, r, checkX, checkY int32) int32 {

    if !inRect(checkX, checkY, w, h) {
        return -1
    }

    if (sdlGet(checkX, checkY, ss) & ColDebug) != 0 {     // already checked this point?
        return -1
    }

    count := removeCount(ss, w, h, checkX, checkY, r)
    if count < 0 {
        return -1        // material would be removed
    }

    sdlSet(checkX, checkY, ColDebug, ss)
    if checkX % 5 == 0 && checkY % 5 == 0 {
        ss.Flip()
    }

    return findPath(ss, checkX, checkY, tX, tY, w, h, r)
}*/

var (
	DistMax = int32(0x1fffffff)
)

func dumpDists(d []int32) {
	if d[0] != DistMax {
		fmt.Printf("%2d ", d[0])
	} else {
		fmt.Printf("   ")
	}
	if d[1] != DistMax {
		fmt.Printf("%2d ", d[1])
	} else {
		fmt.Printf("   ")
	}
	if d[2] != DistMax {
		fmt.Printf("%2d ", d[2])
	} else {
		fmt.Printf("   ")
	}
	if d[3] != DistMax {
		fmt.Printf("%2d ", d[3])
	} else {
		fmt.Printf("   ")
	}
}

func bestDist(d []int32) int {

	if d[0] <= d[1] && d[0] <= d[2] && d[0] <= d[3] {
		return 0
	}
	if d[1] <= d[2] && d[1] <= d[3] {
		return 1
	}
	if d[2] <= d[3] {
		return 2
	}
	return 3
}

func bestDistVal(d []int32) int32 {
    return d[bestDist(d)]
}

// Set distance of nearby points (from point ax,ay -> bx,by) in given dir (0=N,
// 1=S, 2=E, 3=W)
func setDist(ss *sdl.Surface, dist [][][]int32, aX, aY, bX, bY, dir, w, h, r, currBestDist int32, done bool) bool {

    if !inRect(bX, bY, w, h) {
        return done
    }

	dA := dist[aX][aY]
	dB := dist[bX][bY]

	best := bestDistVal(dA) + 1

	if currBestDist >= currBestDist {       // we alredy have better distance
        return done
    }

	if best < dB[dir] && removeCount(ss, w, h, bX, bY, r) != -1 {       // part of model would be removed

        sdlSet(aX, aY, ColDebug, ss)

		dB[dir] = best
		return false
	}
	return done
}

// Find path from cX,cY to tX,tY so that no part of model is removed
func findPath(ss *sdl.Surface, cX, cY, tX, tY, w, h, r int32) bool {

	fmt.Printf("findPath tX=%d tY=%d\n", tX, tY)

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

			dx := abs(x - tx)
			dy := abs(y - ty)
			if x == tx && y == ty {
				dist[x][y] = []int32{0, 0, 0, 0}
			} else if dx+dy == 1 {
				dist[x][y] = []int32{1, 1, 1, 1}
			} else {
				dist[x][y] = []int32{DistMax, DistMax, DistMax, DistMax}
			}
		}
	}

	for {
		done := true
		currBestDist := DistMax
		for x, y, a, ok := nearRectBegin(tX, tY, w, h); ok; x, y, a, ok = nearRectNext(tX, tY, x, y, a, w, h) {

            if abs32(x - cX) >= currBestDist || abs32(y - cY) >= currBestDist {
                continue
            }
            currBestDist = bestDistVal(dist[cX][cY])

            
/*			for i := tY - 3; i <= tY+3; i++ {
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
            

            if x % 16 == 0 && y % 16 == 0 {
                ss.Flip()
            }
            
			done = setDist(ss, dist, x, y, x, y+1, dirN, w, h, r, currBestDist, done)
			done = setDist(ss, dist, x, y, x, y-1, dirS, w, h, r, currBestDist, done)
			done = setDist(ss, dist, x, y, x-1, y, dirE, w, h, r, currBestDist, done)
			done = setDist(ss, dist, x, y, x+1, y, dirW, w, h, r, currBestDist, done)
		}

        fmt.Printf("currBestDist=%d\n", currBestDist)
		
        undrawDebug(ss, w, h)

		
		if done {
			break
		}
	}
	
	for x,y := cX, cY; x != tX || y != tY; {

            for i := x - 3; i <= x+3; i++ {
                for j := y - 3; j <= y+3; j++ {
                    if !inRect(i, j, w, h) {
                        continue
                    }
                    dumpDists(dist[j][i])
                    fmt.Printf("| ")
                }
                fmt.Println()
            }
            fmt.Scanln()
        
        dir := bestDist(dist[x][y])
        if dir == 0 {
            y--
        }
        
        removeMaterial(ss, w, h, x, y, r)
                ss.Flip()
    }

	return true

	/*
	   fmt.Printf("findPath x=%d y=%d tX=%d tY=%d\n", x, y, tX, tY)
	   fmt.Scanln()

	   if x == tX && y == tY {
	       fmt.Printf("hotovo!\n")
	       return 0
	   }

	   if !inRect(x, y, w, h) {
	       return -1
	   }

	   if (sdlGet(x, y, ss) & ColDebug) != 0 {     // already checked this point?
	       return -1
	   }

	   count := removeCount(ss, w, h, x, y, r)
	   if count < 0 {
	       return -1        // material would be removed
	   }

	   sdlSet(x, y, ColDebug, ss)
	   if x % 16 == 0 && y % 16 == 0 {
	       ss.Flip()
	   }

	   cW := findPath(ss, x - 1, y, tX, tY, w, h, r)
	   cE := findPath(ss, x + 1, y, tX, tY, w, h, r)
	   cS := findPath(ss, x, y - 1, tX, tY, w, h, r)
	   cN := findPath(ss, x, y + 1, tX, tY, w, h, r)

	   if cW < cE && cW < cS && cW < cN && cW >= 0 {
	       return cW
	   }
	   if cE < cS && cE < cN && cE >= 0 {
	       return cE
	   }
	   if cS < cN && cS >= 0 {
	       return cS
	   }
	   if cN >= 0 {
	       return cN
	   }
	   return -1
	*/

	// Straight line to target not removing model part
	//x, y := moveOnLine(ss, x, y, tX, tY, w, h, r)

	//if newX == tX && newY == tY {
	//return true
	//}

	/*
		    //fmt.Scanln()
		    drawLine(ss, x, y, tX, tY)
		    ss.Flip()
		    //fmt.Scanln()

		    // Try segment of two lines
		    for mx, my, a, ok := nearRectBegin(x, y, w, h); ok; mx, my, a, ok = nearRectNext(x, y, mx, my, a, w, h) {

		        if my % 10 != 0 || mx % 10 != 0 {
		            continue
		        }

		        //drawLine(ss, x, y, mx, my)
		        //drawLine(ss, mx, my, tX, tY)

		//        fmt.Scanln()
		        ss.Flip()


		        if doLine(ss, x, y, mx, my, w, h, r, true) && 
		            doLine(ss, mx, my, tX, tY, w, h, r, true) {
		                return doLine(ss, x, y, mx, my, w, h, r, false) && 
		                       doLine(ss, mx, my, tX, tY, w, h, r, false)
		            }
		    }*/
}

func move(ss *sdl.Surface, x, y, tX, tY, w, h, r int32) bool {
	return true
}

func main() {

	var r int32 = 16

	img, w, h := pngLoad("case1.png") // image
	ss := sdlInit(w, h)               // sdl surface
	sdlFill(ss, w, h, ColMaterial)    // we have all material in the begining then we remove the parts so that just model is left
	drawModel(img, ss, w, h)          // draw the model with green so that we see if we are removing correct parts

	var x, y int32 = 0, 0

	// Start searching point to remove in where is material - this will fastly
	// remove most of the material. Then we switch to visited mode - this will
	// search in find2Remove also in removed material and will remove the small
	// remaining parts
	colMask := ColRemoved
	for {
		ok, tX, tY := find2Remove(ss, x, y, w, h, r, 0, colMask)

		fmt.Printf("find2Remove x=%d y=%d tX=%d tY=%d ok=%t\n", x, y, tX, tY, ok)

		if !ok {
			if colMask == ColRemoved {
				colMask = ColVisited
				continue
			}
			break
		}

		if !findPath(ss, x, y, tX, tY, w, h, r) {
			fmt.Printf("yay!!\n")
			fmt.Scanln()
			continue
		}

		x, y = tX, tY
		//ss.Flip()

		removeMaterial(ss, w, h, x, y, r)
		ss.Flip()

		for {
			countN := removeCount(ss, w, h, x, y-1, r)
			countS := removeCount(ss, w, h, x, y+1, r)
			countE := removeCount(ss, w, h, x+1, y, r)
			countW := removeCount(ss, w, h, x-1, y, r)

			countNE := removeCount(ss, w, h, x+1, y-1, r)
			countSE := removeCount(ss, w, h, x+1, y+1, r)
			countSW := removeCount(ss, w, h, x-1, y+1, r)
			countNW := removeCount(ss, w, h, x-1, y-1, r)

			if bestDir(countN, countS, countE, countW, countNE, countSE, countSW, countNW) {
				y--
			} else if bestDir(countS, countN, countE, countW, countNE, countSE, countSW, countNW) {
				y++
			} else if bestDir(countE, countN, countS, countW, countNE, countSE, countSW, countNW) {
				x++
			} else if bestDir(countW, countN, countE, countS, countNE, countSE, countSW, countNW) {
				x--
			} else if bestDir(countNE, countN, countS, countE, countW, countSE, countSW, countNW) {
				x++
				y--
			} else if bestDir(countSE, countN, countS, countE, countW, countNE, countSW, countNW) {
				x++
				y++
			} else if bestDir(countSW, countN, countS, countE, countW, countNE, countSE, countNW) {
				x--
				y++
			} else if bestDir(countNW, countN, countS, countE, countW, countNE, countSE, countSW) {
				x--
				y--
			} else {
				fmt.Printf("no good dir %d %d %d %d %d %d %d %d\n", countN, countS, countE, countW, countNE, countSE, countSW, countNW)
				break
			}

			removeMaterial(ss, w, h, x, y, r)
			ss.Flip()
		}
	}

	fmt.Printf("done!\n")

	for true {
	}

	/*
	   var x, y, xx, yy int32    

	   for y = 0; y < height; y++ {

	       ss.Flip();

	       for x = 0; x < width; x++ {
	           if pngIsMaterial(x, y, img) {
	               continue
	           }

	           //fmt.Printf("%d, %d\n", x, y) 
	           //sdlSet(x,y, 0xff,ss)

	           ok := true

	           for xx = x - r; xx <= x + r; xx++ {

	               if xx < 0 || xx >= width {
	                   continue
	               }                

	               for yy = y - r; yy <= y + r; yy++ {
	                   if yy < 0 || yy >= height {
	                       continue
	                   }

	                   if !inRadius(xx, yy, x, y, r) {
	                       continue
	                   }

	                   if pngIsMaterial(xx, yy, img) {
	                       ok = false
	                       //sdlSet(xx,yy, 0xff00,ss)      // milling material that should stay
	                   }
	               }
	           }

	           if ok {

	               for xx = x - r; xx <= x + r; xx++ {

	                   if xx < 0 || xx >= width {
	                       continue
	                   }                

	                   for yy = y - r; yy <= y + r; yy++ {
	                       if yy < 0 || yy >= height {
	                           continue
	                       }                        
	                       if inRadius(xx, yy, x, y, r) {
	                           sdlSet(xx,yy, 0xffff,ss)
	                       }
	                   }
	               }
	           }            
	       }
	   }


	       ss.Flip();

	                   for true {
	                   }
	*/
	/*var n int32;
	  for n=0;n<1000000;n++ {

	    var y int32 =rand.Int31()%480;
	    var x int32 =rand.Int31()%640;
	    var value uint32 = rand.Uint32();
	    sdlSet(x,y,value,ss);

	    ss.Flip();*/
}
