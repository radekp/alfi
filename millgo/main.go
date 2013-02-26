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
	ColMaterial     = uint32(0x000000)
	ColModel        = uint32(0x00ff00)
	ColRemoved      = uint32(0xff0000)
	ColExtraRemoved = uint32(0x0000ff)
	ColVisited      = uint32(0x0000ff)
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

func sdlGet(x int32, y int32, ss *sdl.Surface) uint32 {
	var pix = uintptr(ss.Pixels)
	pix += (uintptr)((y*ss.W)+x) * 4
	var pu = unsafe.Pointer(pix)
	var pp *uint32
	pp = (*uint32)(pu)
	return *pp
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
	ok = inRect(x, y, w, h)
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

// Find a x, y where is material to be removed
func find2Remove(ss *sdl.Surface, currX, currY, w, h, r int32, skipCol uint32) (bool, int32, int32) {

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
			return true, cx, cy
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

func main() {

	var r int32 = 6

	img, w, h := pngLoad("case1.png") // image
	ss := sdlInit(w, h)               // sdl surface
	sdlFill(ss, w, h, ColMaterial)    // we have all material in the begining then we remove the parts so that just model is left
	drawModel(img, ss, w, h)          // draw the model with green so that we see if we are removing correct parts

	var ok bool
	var x, y int32 = 0, 0

	// Start searching point to remove in where is material - this will fastly
	// remove most of the material. Then we switch to visited mode - this will
	// search in find2Remove also in removed material and will remove the small
	// remaining parts
	colMask := ColRemoved
	for {
		ok, x, y = find2Remove(ss, x, y, w, h, r, colMask)

		fmt.Printf("find2Remove x=%d y=%d ok=%t\n", x, y, ok)

		if !ok {
			if colMask == ColRemoved {
				colMask = ColVisited
				continue
			}
			break
		}
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
