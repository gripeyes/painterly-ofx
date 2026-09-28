#include "plugins/ChromaDiffusion.h"
#include "plugins/DetailCollapse.h"
#include "plugins/Pigment.h"

namespace OFX::Plugin {

void getPluginIDs(OFX::PluginFactoryArray& ids) {
  static pigment::plugin::ChromaDiffusionFactory factory(
      "org.painterlyofx.ChromaDiffusion", 1, 0);
  ids.push_back(&factory);
  static pigment::plugin::DetailCollapseFactory detailCollapse(
      "org.painterlyofx.DetailCollapse", 1, 0);
  ids.push_back(&detailCollapse);
  static pigment::plugin::PigmentFactory pigment(
      "org.painterlyofx.Pigment", 1, 0);
  ids.push_back(&pigment);
}

}  // namespace OFX::Plugin
