package main
 
import (
"github.com/0xe2-0x9a-0x9b/Go-SDL/sdl"
"os"
"fmt"
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
)

func pngLoad(fname string) image.Image {
    fmt.Printf("loading %s\n", fname)
    
    file, err := os.Open(fname)
    if err != nil {
        panic(err)
    }
    defer file.Close()

    pic, err := png.Decode(file)
    if err != nil {
        panic(err)
    }
    return pic
}

func pngIsMaterial(x, y int32, img image.Image) bool {
    col := img.At(int(x), int(y))
    r, g, b, a := col.RGBA()
    return r + g + b + a < 32768 * 3 
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

func sdlSet(x int32,y int32,value uint32,screen* sdl.Surface) {
  var pix = uintptr(screen.Pixels);
  pix += (uintptr)((y*screen.W)+x)*unsafe.Sizeof(value);
  var pu = unsafe.Pointer(pix);
  var pp *uint32;
  pp = (*uint32)(pu);
  *pp = value;
}

// Return true if point x,y is inside of cirlcle with center cz,cy and radius r
func inRange(x, y, cx, cy, r int32) bool {
    var dx int32 = cx - x
    var dy int32 = cy - y
    return dx * dx + dy * dy <= r * r
}

func main() {
 
    pic := pngLoad("case1.png")

    b := pic.Bounds()
    var width, height int32 = int32(b.Max.X - b.Min.X), int32(b.Max.Y - b.Min.Y)
    
    var screen = sdlInit(width, height)

    sdlFill(screen, width, height, ColMaterial)
    
    var x, y, xx, yy int32    
    for y = 0; y < height; y++ {
        for x = 0; x < width; x++ {
            if pngIsMaterial(x, y, pic) {
                sdlSet(x,y, 0xff0000,screen);
            }
        }
    }
    
    var r int32 = 15
    
    for y = 0; y < height; y++ {

        screen.Flip();

        for x = 0; x < width; x++ {
            if pngIsMaterial(x, y, pic) {
                continue
            }

            //fmt.Printf("%d, %d\n", x, y) 
            //sdlSet(x,y, 0xff,screen)

            ok := true
            
            for xx = x - r; xx <= x + r; xx++ {
                
                if xx < 0 || xx >= width {
                    continue
                }                
            
                for yy = y - r; yy <= y + r; yy++ {
                    if yy < 0 || yy >= height {
                        continue
                    }
                    
                    if !inRange(xx, yy, x, y, r) {
                        continue
                    }
                    
                    if pngIsMaterial(xx, yy, pic) {
                        ok = false
                        //sdlSet(xx,yy, 0xff00,screen)      // milling material that should stay
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
                        if inRange(xx, yy, x, y, r) {
                            sdlSet(xx,yy, 0xffff,screen)
                        }
                    }
                }
            }            
        }
    }
                
         
        screen.Flip();

                    for true {
                    }
   
  /*var n int32;
  for n=0;n<1000000;n++ {
 
    var y int32 =rand.Int31()%480;
    var x int32 =rand.Int31()%640;
    var value uint32 = rand.Uint32();
    sdlSet(x,y,value,screen);
 
    screen.Flip();*/
}