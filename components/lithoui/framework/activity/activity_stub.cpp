#include "framework/activity/activity.hpp"
#include "framework/activity/activity_manager.hpp"
#include "framework/intent/intent.hpp"

namespace litho {

void Activity::startActivity(Intent& intent) {
    manager().startActivity(intent);
}
void Activity::finish() {
    manager().finishActivity(this);
}

} // namespace litho
