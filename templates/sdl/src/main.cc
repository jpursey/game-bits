#include <thread>

#include "SDL.h"
#include "SDL_main.h"
#include "glog/logging.h"

bool MainLoop() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    LOG(ERROR) << "Unable to initialize SDL: " << SDL_GetError();
    return false;
  }

  auto window =
      SDL_CreateWindow("SDL Template", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    LOG(ERROR) << "Could not create SDL window.";
    return false;
  }

  SDL_Event event;
  bool done = false;
  while (!done) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        done = true;
        break;
      }
    }
    // TODO: Do work...
    std::this_thread::yield();
  }

  if (window != nullptr) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
  return true;
}

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  FLAGS_alsologtostderr = 1;
  google::InitGoogleLogging(argv[0]);
  google::EnableLogCleaner(1);
  return MainLoop() ? 0 : 1;
}
