package main
 
import (
"github.com/0xe2-0x9a-0x9b/Go-SDL/sdl"
"os"
"log"
"fmt"
"unsafe"
//"math/rand"
"image"
"image/png"
)
 
func draw_point(x int32,y int32,value uint32,screen* sdl.Surface) {
  var pix = uintptr(screen.Pixels);
  pix += (uintptr)((y*screen.W)+x)*unsafe.Sizeof(value);
  var pu = unsafe.Pointer(pix);
  var pp *uint32;
  pp = (*uint32)(pu);
  *pp = value;
}

func pixSet(x, y int32, img image.Image) bool {
    col := img.At(int(x), int(y))
    r, g, b, a := col.RGBA()
    return r + g + b + a > 32768 * 3 
}

// Return true if point x,y is inside of cirlcle with center cz,cy and radius r
func inRange(x, y, cx, cy, r int32) bool {
    var dx int32 = cx - x
    var dy int32 = cy - y
    return dx * dx + dy * dy <= r * r
}

func main() {
 
  var fname string
  fname = "case1.png"
    file, err := os.Open(fname)
    if err != nil {
        fmt.Println(err)
        return
    }
    defer file.Close()

    pic, err := png.Decode(file)
    if err != nil {
        fmt.Fprintf(os.Stderr, "%s: %v\n", fname, err)
        return
    }

    b := pic.Bounds()
    var width, height int32 = int32(b.Max.X - b.Min.X), int32(b.Max.Y - b.Min.Y)
    var x, y, xx, yy int32
    
    var screen = sdl.SetVideoMode(int(width), int(height), 32, sdl.RESIZABLE)
 
    if screen == nil {
        log.Fatal(sdl.GetError())
    }

    
    for y = 0; y < height; y++ {
        for x = 0; x < width; x++ {
            if !pixSet(x, y, pic) {
                draw_point(x,y, 0xff0000,screen);
            }
        }
    }
    
    var r int32 = 15
    
    for y = 0; y < height; y++ {

        screen.Flip();

        for x = 0; x < width; x++ {
            if !pixSet(x, y, pic) {
                continue
            }

            //fmt.Printf("%d, %d\n", x, y) 
            //draw_point(x,y, 0xff,screen)

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
                    
                    if !pixSet(xx, yy, pic) {
                        ok = false
                        //draw_point(xx,yy, 0xff00,screen)      // milling material that should stay
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
                            draw_point(xx,yy, 0xffff,screen)
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
    draw_point(x,y,value,screen);
 
    screen.Flip();*/
}