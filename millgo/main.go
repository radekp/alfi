package main
 
import (
"github.com/0xe2-0x9a-0x9b/Go-SDL/sdl"
"os"
"log"
"fmt"
"unsafe"
//"math/rand"
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
 
func main() {
 
  var screen = sdl.SetVideoMode(640, 480, 32, sdl.RESIZABLE)
 
  if screen == nil {
    log.Fatal(sdl.GetError())
  }
  
  
  
  var fname string
  fname = "/usr/share/icons/hicolor/128x128/apps/vlc.png"
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

    for y := b.Min.Y; y < b.Max.Y; y++ {
        for x := b.Min.X; x < b.Max.X; x++ {
            col := pic.At(x, y)
            
            r, g, b, a := col.RGBA()
                draw_point(int32(x),int32(y),r + g + b + a,screen);
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