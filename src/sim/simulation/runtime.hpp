#pragma once

#include "sim/simulation/constants.hpp"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <mutex>

namespace scraperx::sim::detail {

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterface() {
        mapping_[object_layers::kStatic] = broadphase_layers::kStatic;
        mapping_[object_layers::kMoving] = broadphase_layers::kMoving;
    }

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
        return broadphase_layers::kCount;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
        const JPH::ObjectLayer layer) const override {
        JPH_ASSERT(layer < object_layers::kCount);
        return mapping_[layer];
    }

    [[nodiscard]] const char *GetBroadPhaseLayerName(
        const JPH::BroadPhaseLayer layer) const override {
        if (layer == broadphase_layers::kStatic) {
            return "STATIC";
        }
        if (layer == broadphase_layers::kMoving) {
            return "MOVING";
        }
        JPH_ASSERT(false);
        return "INVALID";
    }

private:
    JPH::BroadPhaseLayer mapping_[object_layers::kCount];
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer object_layer,
                                     const JPH::BroadPhaseLayer broadphase_layer) const override {
        if (object_layer == object_layers::kStatic) {
            return broadphase_layer == broadphase_layers::kMoving;
        }
        if (object_layer == object_layers::kMoving) {
            return true;
        }
        JPH_ASSERT(false);
        return false;
    }
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer first,
                                     const JPH::ObjectLayer second) const override {
        if (first == object_layers::kStatic) {
            return second == object_layers::kMoving;
        }
        if (first == object_layers::kMoving) {
            return true;
        }
        JPH_ASSERT(false);
        return false;
    }
};

class JoltRuntimeLease final {
public:
    JoltRuntimeLease() {
        const std::scoped_lock lock(mutex_);
        if (lease_count_++ == 0) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    ~JoltRuntimeLease() {
        const std::scoped_lock lock(mutex_);
        if (--lease_count_ == 0) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

    JoltRuntimeLease(const JoltRuntimeLease &) = delete;
    JoltRuntimeLease &operator=(const JoltRuntimeLease &) = delete;

private:
    static std::mutex mutex_;
    static std::uint32_t lease_count_;
};



} // namespace scraperx::sim::detail
