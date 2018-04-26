#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "halide_image.h"
#include "halide_image_io.h"
#include "pipeline_native.h"
//#include "pipeline_hls.h"

#include "coreir.h"
#include "coreir/passes/transform/rungenerators.h"
#include "coreir/simulator/interpreter.h"
#include "coreir/libs/commonlib.h"

using namespace Halide::Tools;
using namespace CoreIR;

template <typename elem_t>
class ImageWriter {
public:
  ImageWriter(uint32_t width, uint32_t height, uint32_t channels=1) :
    width(width), height(height), channels(channels),
    current_x(0), current_y(0), current_z(0) { }

  void write(Image<elem_t> &image, elem_t data) {
    if (!(current_x < width &&
          current_y < height &&
          current_z < channels)) {
      std::cout << "wrote too many: " 
                << current_x << "," << current_y << "," << current_z << std::endl;      
    }
    image(current_x, current_y, current_z) = data;

    // increment coords
    current_x++;
    if (current_x == width) {
      current_y++;
      current_x = 0;
    }
    if (current_y == height) {
      current_z++;
      current_y = 0;
    }
  }

  elem_t read(Image<elem_t> &image, uint x, uint y, uint z) {
    if (!(x < width &&
          y < height &&
          z < channels)) {
      std::cout << "read out of bounds: " << x << "," << y << "," << z << std::endl;      
    }

    return image(x,y,z);
  }

  void save_image(Image<elem_t> &image, std::string image_name) {
    Halide::Tools::save_image(image, image_name);
  }

  void print_coords() {
    std::cout << "x=" << current_x << ",y=" << current_y << ",z=" << current_z << std::endl;
  }

private:
  const uint32_t width, height, channels;
  uint32_t current_x, current_y, current_z;
};

int main(int argc, char **argv) {
  Image<uint8_t> in(64,64, 1);

  Image<uint8_t> out_native(in.width()-4, in.height()-4, in.channels());
  //Image<uint8_t> out_hls(in.width(), in.height(), in.channels());
  Image<uint8_t> coreir_image(in.width()-4, in.height()-4, in.channels());
  Image<uint8_t> out_coreir(in.width(), in.height(), in.channels());


  ImageWriter<uint8_t> *coreir_writer = 
    new ImageWriter<uint8_t>(coreir_image.width(), coreir_image.height(), coreir_image.channels());

  
  in = load_image(argv[1]);
  save_image(in, "input_crop.png");
  
  printf("start.\n");
  
  pipeline_native(in, out_native);
  save_image(out_native, "out.png");
  
  printf("finish running native code\n");
  
  /*pipeline_hls(in, 0, out_hls);
  
    printf("finish running HLS code\n");
  
    bool success = true;
    for (int y = 0; y < out_native.height(); y++) {
    for (int x = 0; x < out_native.width(); x++) {
    for (int c = 0; c < out_native.channels(); c++) {
    if (out_native(x, y, c) != out_hls(x, y, c)) {
    printf("out_native(%d, %d, %d) = %d, but out_c(%d, %d, %d) = %d\n",
    x, y, c, out_native(x, y, c),
    x, y, c, out_hls(x, y, c));
    success = false;
  */
  bool success = true;
  
  // New context for coreir test
  Context* c = newContext();
  Namespace* g = c->getGlobal();

  CoreIRLoadLibrary_commonlib(c);
  if (!loadFromFile(c,"./design_prepass.json")) {
    std::cout << "Could not Load from json!!" << std::endl;
    c->die();
  }

  c->runPasses({"rungenerators", "flattentypes", "flatten", "wireclocks-coreir"});

  Module* m = g->getModule("DesignTop");
  assert(m != nullptr);
  SimulatorState state(m);

  state.setValue("self.in_arg_1_0_0", BitVector(16));
  state.resetCircuit();
  state.setClock("self.clk", 0, 1);

  std::cout << "Starting coreir simulation" << std::endl;

  for (int y = 0; y < in.height(); y+=1) {
    for (int x = 0; x < in.width(); x+=1) {
      for (int c = 0; c < in.channels(); c++) {
        // set input value
        state.setValue("self.in_arg_1_0_0", BitVector(16, in(x,y,c)));
        // propogate to all wires
        state.exeCombinational();
        state.exeCombinational();

            
        // read output wire
        out_coreir(x,y,c) = state.getBitVec("self.out_0_0").to_type<uint8_t>();
        //uint16_t coreir_value = state.getBitVec("self.out_0_0").to_type<uint16_t>();
        if (x>=4 && y>=4 && out_native(x-4, y-4, c) != out_coreir(x,y,c)) {
          printf("out_native(%d, %d, %d) = %d, but out_coreir(%d, %d, %d) = %d\n",
                 x-4, y-4, c, out_native(x-4, y-4, c),
                 x, y, c, out_coreir(x,y,c));
          success = false;
        }


        bool valid = state.getBitVec("self.valid").to_type<bool>();
        if (x>=4 && y>=4 && !valid) {
          printf("out_native(%d, %d, %d) = %d and out_coreir(%d, %d, %d) = %d, but valid = %d\n",
                 x-4, y-4, c, out_native(x-4, y-4, c),
                 x, y, c, out_coreir(x,y,c), valid);
        }
        if (valid) {
          coreir_writer->write(coreir_image,out_coreir(x,y,c));
        }
        //printf("out_coreir(%d, %d, %d) = %d,  valid=%d, first_pixel=%d\n",x,y,c,out_coreir(x,y,c), valid, coreir_writer->read(0,0,0));

        //printf("out_coreir(%d,%d,%d) = %d\n",  x,y,c,out_coreir(x,y,c));

        // printout state
        /*
          printf("cycle%d:  in=%d, mid=%d, out=%d\n", 
          c + x*in.channels() + y*in.width()*in.channels(),
          in(x,y,c),
          state.getBitVec("add_495_526_527.out").to_type<uint16_t>(),
          out_coreir(x,y,c));

          std::cout << "cycle" << c + x*in.channels() + y*in.width()*in.channels()
          << ":  in=" << in(x,y,c) 
          << ", mid=" << state.getBitVec("add_519_523_524.out").to_type<uint16_t>()
          << ", out=" << out_coreir(x,y,c) << std::endl;
        */

        // give another rising edge (execute seq)
        state.exeSequential();
      }
    }
  }

  coreir_writer->save_image(coreir_image,"out_coreir.png");
  coreir_writer->print_coords();

  for (int y = 0; y < out_native.height(); y++) {
    for (int x = 0; x < out_native.width(); x++) {
	    for (int c = 0; c < out_native.channels(); c++) {
        if (out_native(x, y, c) != coreir_writer->read(coreir_image,x, y, c)) {
          printf("out_native(%d, %d, %d) = %d, but out_coreir(%d, %d, %d) = %d\n",
                 x, y, c, out_native(x, y, c),
                 x, y, c, coreir_writer->read(coreir_image,x, y, c));
          success = false;
        }
      }
    }
  }

  deleteContext(c);
  if (success) {
    printf("Succeeded!\n");
    return 0;
  } else {
    printf("Failed!\n");
    return 1;
  }

}
