#include "constants.hpp"

#ifdef PALLET_CONSTANTS_PLATFORM_IS_POSIX

#include <memory>
#include <thread>
#include <vector>
#include <array>
#include <atomic>

#include "SDL3/SDL.h"

#include "pallet/GraphicsInterface.hpp"
#include "pallet/containers/ThreadSafeStack.hpp"
#include "pallet/error.hpp"
#include "pallet/PosixPlatform.hpp"
#include "pallet/posix.hpp"

namespace pallet {

class SDLHardwareInterface final : public GraphicsHardwareInterface {
public:

  void(*onEventsCallback)(SDL_Event* events, size_t len, void* ud) = nullptr;
  void* onEventsUserData;
  unsigned int userEventType;
  int scaleFactor = 9;

  void config(int scaleFactor = 9) {
    this->scaleFactor = scaleFactor;
  }

  void init() override;
  void clear() override;
  void render() override;
  void rect(float x, float y, float w, float h, int c) override;
  void point(float x, float y, int c) override;
  void loop();
  void stopLoop();
  void cleanup();

  SDLHardwareInterface() :
    shouldRunLoop(std::make_unique<std::atomic<bool>>(true)) {};
  // SDLHardwareInterface(SDLHardwareInterface&& other);

private:

  bool sdlInited = false;
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  std::unique_ptr<std::atomic<bool>> shouldRunLoop;

};

using namespace detail;



namespace detail {

// This is being done to avoid having to write move operators for
// PosixGraphicsInterface. We just need to stop the loop for that the
// jthread join works!
struct SDLHardwareInterfaceStopperDeleter {
  void operator()(SDLHardwareInterface* sdlHardwareInterface) {
    sdlHardwareInterface->stopLoop();
  }
};

using SDLHardwareInterfaceStopper = std::unique_ptr<SDLHardwareInterface, SDLHardwareInterfaceStopperDeleter>;
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
  PosixPlatform* platform {};
  FdManager pipeFdManager;
  SDLHardwareInterface sdlHardwareInterface;
  std::jthread thrd;
  SDLHardwareInterfaceStopper sdlHardwareInterfaceStopper;
  std::unique_ptr<std::vector<Operation>> operationsBuffer;
  containers::ThreadSafeStack<std::unique_ptr<std::vector<Operation>>> operationVectorStack;
  pallet::Pipe pipe;
  bool hardwareInterfaceRunning = false;

  void uponPipeIn(void* datain, size_t len);
  void renderOperations(std::vector<Operation>& operations);
  void uponEventsGThread(SDL_Event* events, size_t len);
  void uponUserEventGThread(SDL_Event* event);
  void addOperation(auto&& op);

};

 static_assert(std::is_move_constructible_v<PosixGraphicsInterface>);


}

#endif
