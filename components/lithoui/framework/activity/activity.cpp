#include "framework/activity/activity.hpp"
#include "framework/activity/activity_manager.hpp"

namespace litho {

void Activity::startActivity(Intent& intent) {
    if (mManager) mManager->startActivity(intent);
}

void Activity::startActivity(Intent& intent, const TransitionSpec& spec) {
    if (mManager) mManager->startActivity(intent, spec);
}

void Activity::finish() {
    if (mManager) mManager->finishActivity(this);
}

void Activity::finish(const TransitionSpec& spec) {
    if (mManager) mManager->finishActivity(this, spec);
}

} // namespace litho
