#include "plugins/ChromaDiffusion.h"

namespace OFX::Plugin {

void getPluginIDs(OFX::PluginFactoryArray& ids) {
  static pigment::plugin::ChromaDiffusionFactory factory(
      "org.painterlyofx.ChromaDiffusion", 1, 0);
  ids.push_back(&factory);
}

}  // namespace OFX::Plugin

