// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "flutter/shell/platform/windows/flutter_platform_node_delegate_windows.h"

#include "flutter/shell/platform/windows/accessibility_bridge_windows.h"
#include "flutter/shell/platform/windows/flutter_windows_view.h"
#include "flutter/third_party/accessibility/ax/ax_clipping_behavior.h"
#include "flutter/third_party/accessibility/ax/ax_coordinate_system.h"
#include "flutter/third_party/accessibility/ax/platform/ax_fragment_root_win.h"

namespace flutter {

FlutterPlatformNodeDelegateWindows::FlutterPlatformNodeDelegateWindows(
    std::weak_ptr<AccessibilityBridge> bridge,
    FlutterWindowsView* view)
    : bridge_(bridge), view_(view) {
  assert(!bridge_.expired());
  assert(view_);
}

FlutterPlatformNodeDelegateWindows::~FlutterPlatformNodeDelegateWindows() {
  if (ax_platform_node_) {
    ax_platform_node_->Destroy();
  }
}

// |ui::AXPlatformNodeDelegate|
void FlutterPlatformNodeDelegateWindows::Init(std::weak_ptr<OwnerBridge> bridge,
                                              ui::AXNode* node) {
  FlutterPlatformNodeDelegate::Init(bridge, node);
  ax_platform_node_ = ui::AXPlatformNode::Create(this);
  assert(ax_platform_node_);
}

// |ui::AXPlatformNodeDelegate|
gfx::NativeViewAccessible
FlutterPlatformNodeDelegateWindows::GetNativeViewAccessible() {
  assert(ax_platform_node_);
  return ax_platform_node_->GetNativeViewAccessible();
}

// |ui::AXPlatformNodeDelegate|
gfx::NativeViewAccessible FlutterPlatformNodeDelegateWindows::HitTestSync(
    int screen_physical_pixel_x,
    int screen_physical_pixel_y) const {
  // If this node doesn't contain the point, return.
  ui::AXOffscreenResult result;
  gfx::Rect rect = GetBoundsRect(ui::AXCoordinateSystem::kScreenPhysicalPixels,
                                 ui::AXClippingBehavior::kUnclipped, &result);
  gfx::Point screen_point(screen_physical_pixel_x, screen_physical_pixel_y);
  if (!rect.Contains(screen_point)) {
    return nullptr;
  }

  // If any child in this node's subtree contains the point, return that child.
  auto bridge = bridge_.lock();
  assert(bridge);
  for (const ui::AXNode* child : GetAXNode()->children()) {
    std::shared_ptr<FlutterPlatformNodeDelegateWindows> win_delegate =
        std::static_pointer_cast<FlutterPlatformNodeDelegateWindows>(
            bridge->GetFlutterPlatformNodeDelegateFromID(child->id()).lock());
    assert(win_delegate);
    auto hit_view = win_delegate->HitTestSync(screen_physical_pixel_x,
                                              screen_physical_pixel_y);
    if (hit_view) {
      return hit_view;
    }
  }

  // If no children contain the point, but this node does, return this node.
  return ax_platform_node_->GetNativeViewAccessible();
}

// |FlutterPlatformNodeDelegate|
gfx::Rect FlutterPlatformNodeDelegateWindows::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  gfx::Rect bounds = FlutterPlatformNodeDelegate::GetBoundsRect(
      coordinate_system, clipping_behavior, offscreen_result);
  POINT origin{bounds.x(), bounds.y()};
  POINT extent{bounds.x() + bounds.width(), bounds.y() + bounds.height()};
  ClientToScreen(view_->GetPlatformWindow(), &origin);
  ClientToScreen(view_->GetPlatformWindow(), &extent);
  return gfx::Rect(origin.x, origin.y, extent.x - origin.x,
                   extent.y - origin.y);
}

void FlutterPlatformNodeDelegateWindows::DispatchWinAccessibilityEvent(
    ax::mojom::Event event_type) {
  ax_platform_node_->NotifyAccessibilityEvent(event_type);
}

void FlutterPlatformNodeDelegateWindows::SetFocus() {
  VARIANT varchild{};
  varchild.vt = VT_I4;
  varchild.lVal = CHILDID_SELF;
  GetNativeViewAccessible()->accSelect(SELFLAG_TAKEFOCUS, varchild);
}

gfx::AcceleratedWidget
FlutterPlatformNodeDelegateWindows::GetTargetForNativeAccessibilityEvent() {
  return view_->GetPlatformWindow();
}

ui::AXPlatformNode* FlutterPlatformNodeDelegateWindows::GetPlatformNode()
    const {
  return ax_platform_node_;
}

}  // namespace flutter
