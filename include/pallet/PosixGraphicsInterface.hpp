#include "constants.hpp"

#ifdef PALLET_CONSTANTS_PLATFORM_IS_POSIX

#include <memory>
#include <thread>
#include <vector>
#include <array>

#include "SDL3/SDL.h"

#include "GraphicsInterface.hpp"
#include "containers/ThreadSafeStack.hpp"
#include "error.hpp"
#include "memory.hpp"
#include "PosixPlatform.hpp"
#include "posix.hpp"

namespace pallet {

class SDLHardwareInterface final : public GraphicsHardwareInterface {

  struct Data {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool inited = false;
  };

  struct DataDestroyer {
    void operator() (Data& data) noexcept {
      if (data.renderer) {
          SDL_DestroyRenderer(data.renderer);
          data.renderer = nullptr;
      }
      if (data.window) {
        SDL_DestroyWindow(data.window);
        data.window = nullptr;
      }

      if (data.inited) {
        data.inited = false;
        SDL_Quit();
      }
    }
  };

  UniqueResource<Data, DataDestroyer> data;


public:

  int scaleFactor = 9;

  void(*onEventsCallback)(SDL_Event* events, size_t len, void* ud) = nullptr;
  void* onEventsUserData = nullptr;
  unsigned int userEventType;

  void config(int scaleFactor = 9) {
    this->scaleFactor = scaleFactor;
  }

  void init() override;
  void clear() override;
  void render() override;
  void rect(float x, float y, float w, float h, int c) override;
  void point(float x, float y, int c) override;
  void loop();
  void close();
};

using namespace detail;

namespace detail {
  struct PipeDataDestroyer {
    void operator()(std::array<int, 2>& pipes) noexcept {
      if (!(pipes[0] < 0)) {
        close(pipes[0]);
      }

      if (!(pipes[1] < 0)) {
        close(pipes[1]);
      }
    }
  };
}

class PosixGraphicsInterface final : public GraphicsInterface {
public:

  static Result<PosixGraphicsInterface> create(PosixPlatform& platform);
  // needs to be public to construct it through Result<...>
  // But I can use PrivateType to prevent use
  PosixGraphicsInterface(PosixPlatform& platform, pallet::Pipe&& pipes);
  PosixGraphicsInterface(PosixGraphicsInterface&& iface);

  virtual void render() override;
  virtual void quit() override;
  virtual void clear() override;
  virtual void rect(float x, float y, float w, float h, int c) override;
  virtual void point(float x, float y, int c) override;
  virtual void text(float x, float y, std::string_view str,
                    int fc = 15, int bc = 0,
                    GraphicsPosition align = GraphicsPosition::Default,
                    GraphicsPosition baseline = GraphicsPosition::Default) override;
  virtual GraphicsTextMeasurement measureText(std::string_view str) override;

private:

  PosixPlatform* platform;
  FdManager pipeFdManager;
  SDLHardwareInterface sdlHardwareInterface;
  std::thread thrd;
  std::unique_ptr<std::vector<Operation>> operationsBuffer = nullptr;
  containers::ThreadSafeStack<std::unique_ptr<std::vector<Operation>>> operationVectorStack;
  pallet::Pipe pipes;

  void uponPipeIn(void* datain, size_t len);
  void renderOperations(std::vector<Operation>& operations);
  void uponEvents(SDL_Event* events, size_t len);
  void uponUserEvent(SDL_Event* event);
  void addOperation(auto&& op);

};

 static_assert(std::is_move_constructible_v<PosixGraphicsInterface>);


}

#endif
