#include "framework/activity/activity.hpp"
#include "framework/activity/activity_manager.hpp"

namespace litho {

void Activity::startActivity(Intent& intent) {
    if (mManager) mManager->startActivity(intent);
}

void Activity::finish() {
    if (mManager) mManager->finishActivity(this);
}

} // namespace litho
