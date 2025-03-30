#include <cstdint>
#include <string_view>
#include <tuple>

#include "pallet/variant.hpp"

#include "pallet/MonomeGridInterface.hpp"


namespace pallet {

static inline int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static inline std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}


MonomeGrid::MonomeGrid(std::string id, int rows, int cols, MonomeGrid::QuadRenderFunction quadRenderFunc,
           void* quadRenderFuncUd0, void* quadRenderFuncUd1) :
  id(std::move(id)), rows(rows), cols(cols), nQuads((rows / 8) * (cols / 8)), quadRenderFunc(quadRenderFunc),
  quadRenderFuncUd0(quadRenderFuncUd0), quadRenderFuncUd1(quadRenderFuncUd1)
{
  memset(&this->quadData[0][0], 0, sizeof(MonomeGrid::QuadType) * 4);
}

void MonomeGrid::setQuadDirty(int quadIndex) {
  this->quadDirtyFlags |= (1 << quadIndex);
}

bool MonomeGrid::isQuadDirty(int quadIndex) {
  return this->quadDirtyFlags & (1 << quadIndex);
}

void MonomeGrid::setOnKey(void (*cb)(int x, int y, int z, void*), void* data) {
  this->onKeyCb = cb;
  this->onKeyData = data;
}

void MonomeGrid::uponKey(int x, int y, int z) {
  if (this->onKeyCb) {
    this->onKeyCb(x, y, z, onKeyData);
  }
}

void MonomeGrid::uponConnectionState(bool state) {
  this->connected = state;
}

bool MonomeGrid::isConnected() {
  return this->connected;
}

void MonomeGrid::led(int x, int y, int c) {
  const auto [quadIndex, pointIndex] = calcQuadIndexAndPointIndex(x, y);
  quadData[quadIndex][pointIndex] = c;
  this->setQuadDirty(quadIndex);
}

void MonomeGrid::all(int z) {
  memset(&quadData, z, sizeof(MonomeGrid::QuadType) * 4);
  for (int i = 0; i < 4; i++) {
    this->setQuadDirty(i);
  }
}

void MonomeGrid::clear() {
  memset(&quadData, 0, sizeof(MonomeGrid::QuadType) * 4);
  for (int i = 0; i < 4; i++) {
    this->setQuadDirty(i);
  }
}

void MonomeGrid::render() {
  if (!this->connected) { return; }
  for (int quadIndex = 0; quadIndex < this->nQuads; quadIndex++) {
    if (this->isQuadDirty(quadIndex)) {
      int offX = (quadIndex % 2) * 8;
      int offY = (quadIndex / 2) * 8;
      this->quadRenderFunc(offX, offY, quadData[quadIndex], this->quadRenderFuncUd0, this->quadRenderFuncUd1);
    }
  }
  this->quadDirtyFlags = 0;
}

int MonomeGrid::getRows() {
  return this->rows;
}

int MonomeGrid::getCols() {
  return this->cols;
}


  void MonomeGridInterface::uponConnectionState(MonomeGrid& g, bool b) {
    g.uponConnectionState(b);
  }
  void MonomeGridInterface::uponKey(MonomeGrid& g, int x, int y, int z) {
    g.uponKey(x, y, z);
  }
}
