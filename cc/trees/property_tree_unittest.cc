// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree.h"

#include "cc/test/geometry_test_utils.h"
#include "cc/trees/draw_property_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PropertyTreeTest, ComputeTransformRoot) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  gfx::Transform expected;
  gfx::Transform transform;
  bool success = tree.ComputeTransform(0, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.Translate(2, 2);
  success = tree.ComputeTransform(0, -1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-2, -2);
  success = tree.ComputeTransform(-1, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformChild) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode child;
  child.data.local.Translate(3, 3);
  child.data.target_id = 0;

  tree.Insert(child, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(3, 3);
  bool success = tree.ComputeTransform(1, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-3, -3);
  success = tree.ComputeTransform(0, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(5, 5);
  success = tree.ComputeTransform(1, -1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-5, -5);
  success = tree.ComputeTransform(-1, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, TransformsWithFlattening) {
  TransformTree tree;

  int grand_parent = tree.Insert(TransformNode(), 0);
  tree.Node(grand_parent)->data.content_target_id = grand_parent;
  tree.Node(grand_parent)->data.target_id = grand_parent;

  gfx::Transform rotation_about_x;
  rotation_about_x.RotateAboutXAxis(15);

  int parent = tree.Insert(TransformNode(), grand_parent);
  tree.Node(parent)->data.needs_sublayer_scale = true;
  tree.Node(parent)->data.target_id = grand_parent;
  tree.Node(parent)->data.content_target_id = parent;
  tree.Node(parent)->data.local = rotation_about_x;

  int child = tree.Insert(TransformNode(), parent);
  tree.Node(child)->data.target_id = parent;
  tree.Node(child)->data.content_target_id = parent;
  tree.Node(child)->data.flattens_inherited_transform = true;
  tree.Node(child)->data.local = rotation_about_x;

  int grand_child = tree.Insert(TransformNode(), child);
  tree.Node(grand_child)->data.target_id = parent;
  tree.Node(grand_child)->data.content_target_id = parent;
  tree.Node(grand_child)->data.flattens_inherited_transform = true;
  tree.Node(grand_child)->data.local = rotation_about_x;

  ComputeTransforms(&tree);

  gfx::Transform flattened_rotation_about_x = rotation_about_x;
  flattened_rotation_about_x.FlattenTo2d();

  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x,
                                  tree.Node(child)->data.to_target);

  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x * rotation_about_x,
                                  tree.Node(child)->data.to_screen);

  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x * rotation_about_x,
                                  tree.Node(grand_child)->data.to_target);

  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_x *
                                      flattened_rotation_about_x *
                                      rotation_about_x,
                                  tree.Node(grand_child)->data.to_screen);

  gfx::Transform grand_child_to_child;
  bool success =
      tree.ComputeTransform(grand_child, child, &grand_child_to_child);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);

  // Remove flattening at grand_child, and recompute transforms.
  tree.Node(grand_child)->data.flattens_inherited_transform = false;
  ComputeTransforms(&tree);

  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x * rotation_about_x,
                                  tree.Node(grand_child)->data.to_target);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      flattened_rotation_about_x * rotation_about_x * rotation_about_x,
      tree.Node(grand_child)->data.to_screen);

  success = tree.ComputeTransform(grand_child, child, &grand_child_to_child);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(rotation_about_x, grand_child_to_child);
}

TEST(PropertyTreeTest, MultiplicationOrder) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode child;
  child.data.local.Scale(2, 2);
  child.data.target_id = 0;

  tree.Insert(child, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  expected.Translate(2, 2);
  expected.Scale(2, 2);

  gfx::Transform transform;
  gfx::Transform inverse;

  bool success = tree.ComputeTransform(1, -1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  success = tree.ComputeTransform(-1, 1, &inverse);
  EXPECT_TRUE(success);

  transform = transform * inverse;
  expected.MakeIdentity();
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformWithUninvertibleTransform) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode child;
  child.data.local.Scale(0, 0);
  child.data.target_id = 0;

  tree.Insert(child, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  expected.Scale(0, 0);

  gfx::Transform transform;
  gfx::Transform inverse;

  bool success = tree.ComputeTransform(1, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  // To compute this would require inverting the 0 matrix, so we cannot
  // succeed.
  success = tree.ComputeTransform(0, 1, &inverse);
  EXPECT_FALSE(success);
}

TEST(PropertyTreeTest, ComputeTransformWithSublayerScale) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode grand_parent;
  grand_parent.data.local.Scale(2.f, 2.f);
  grand_parent.data.target_id = 0;
  grand_parent.data.needs_sublayer_scale = true;
  int grand_parent_id = tree.Insert(grand_parent, 0);
  tree.UpdateTransforms(grand_parent_id);

  TransformNode parent;
  parent.data.local.Translate(15.f, 15.f);
  parent.data.target_id = grand_parent_id;
  int parent_id = tree.Insert(parent, grand_parent_id);
  tree.UpdateTransforms(parent_id);

  TransformNode child;
  child.data.local.Scale(3.f, 3.f);
  child.data.target_id = grand_parent_id;
  int child_id = tree.Insert(child, parent_id);
  tree.UpdateTransforms(child_id);

  TransformNode grand_child;
  grand_child.data.local.Scale(5.f, 5.f);
  grand_child.data.target_id = grand_parent_id;
  grand_child.data.needs_sublayer_scale = true;
  int grand_child_id = tree.Insert(grand_child, child_id);
  tree.UpdateTransforms(grand_child_id);

  EXPECT_EQ(gfx::Vector2dF(2.f, 2.f),
            tree.Node(grand_parent_id)->data.sublayer_scale);
  EXPECT_EQ(gfx::Vector2dF(30.f, 30.f),
            tree.Node(grand_child_id)->data.sublayer_scale);

  // Compute transform from grand_parent to grand_child.
  gfx::Transform expected_transform_without_sublayer_scale;
  expected_transform_without_sublayer_scale.Scale(1.f / 15.f, 1.f / 15.f);
  expected_transform_without_sublayer_scale.Translate(-15.f, -15.f);

  gfx::Transform expected_transform_with_dest_sublayer_scale;
  expected_transform_with_dest_sublayer_scale.Scale(30.f, 30.f);
  expected_transform_with_dest_sublayer_scale.Scale(1.f / 15.f, 1.f / 15.f);
  expected_transform_with_dest_sublayer_scale.Translate(-15.f, -15.f);

  gfx::Transform expected_transform_with_source_sublayer_scale;
  expected_transform_with_source_sublayer_scale.Scale(1.f / 15.f, 1.f / 15.f);
  expected_transform_with_source_sublayer_scale.Translate(-15.f, -15.f);
  expected_transform_with_source_sublayer_scale.Scale(0.5f, 0.5f);

  gfx::Transform transform;
  bool success =
      tree.ComputeTransform(grand_parent_id, grand_child_id, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_without_sublayer_scale,
                                  transform);

  success = tree.ComputeTransformWithDestinationSublayerScale(
      grand_parent_id, grand_child_id, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_with_dest_sublayer_scale,
                                  transform);

  success = tree.ComputeTransformWithSourceSublayerScale(
      grand_parent_id, grand_child_id, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_with_source_sublayer_scale,
                                  transform);

  // Now compute transform from grand_child to grand_parent.
  expected_transform_without_sublayer_scale.MakeIdentity();
  expected_transform_without_sublayer_scale.Translate(15.f, 15.f);
  expected_transform_without_sublayer_scale.Scale(15.f, 15.f);

  expected_transform_with_dest_sublayer_scale.MakeIdentity();
  expected_transform_with_dest_sublayer_scale.Scale(2.f, 2.f);
  expected_transform_with_dest_sublayer_scale.Translate(15.f, 15.f);
  expected_transform_with_dest_sublayer_scale.Scale(15.f, 15.f);

  expected_transform_with_source_sublayer_scale.MakeIdentity();
  expected_transform_with_source_sublayer_scale.Translate(15.f, 15.f);
  expected_transform_with_source_sublayer_scale.Scale(15.f, 15.f);
  expected_transform_with_source_sublayer_scale.Scale(1.f / 30.f, 1.f / 30.f);

  success = tree.ComputeTransform(grand_child_id, grand_parent_id, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_without_sublayer_scale,
                                  transform);

  success = tree.ComputeTransformWithDestinationSublayerScale(
      grand_child_id, grand_parent_id, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_with_dest_sublayer_scale,
                                  transform);

  success = tree.ComputeTransformWithSourceSublayerScale(
      grand_child_id, grand_parent_id, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform_with_source_sublayer_scale,
                                  transform);
}

}  // namespace cc
