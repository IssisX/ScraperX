void build_s2_statics(std::vector<Part> &frame) {
    // The landing from the +33 m ring, over its edge beam, on side beams that carry
    // the bearings and on cross beams at either end.
    frame.push_back(span({-19.50F, kStairApproachTop - 0.10F, -124.05F}, {-15.43F, kStairApproachTop, -120.60F},
                         Material::Galvanised));
    frame.push_back(span({-19.50F, 32.40F, -123.70F}, {-14.20F, 32.90F, -123.40F}, Material::Rust));
    frame.push_back(span({-19.50F, 32.40F, -120.80F}, {-14.20F, 32.90F, -120.58F}, Material::Rust));
    frame.push_back(span({-19.50F, 32.40F, -123.70F}, {-19.25F, 32.90F, -120.58F}, Material::Rust));
    frame.push_back(span({-14.40F, 32.30F, -123.70F}, {-14.20F, 32.80F, -120.58F}, Material::Rust));
    frame.push_back(span({-15.25F, 32.40F, -123.57F}, {-14.75F, kStairApproachTop, -123.29F}, Material::Steel));
    frame.push_back(span({-15.25F, 32.40F, -120.91F}, {-14.75F, kStairApproachTop, -120.63F}, Material::Steel));
    // Rails round the landing's south edge and west end.
    for (const float x : {-19.40F, -18.00F, -16.60F}) {
        frame.push_back(span({x - 0.04F, kStairApproachTop, -120.68F}, {x + 0.04F, kStairApproachTop + 1.05F, -120.60F},
                             Material::Yellow));
    }
    frame.push_back(span({-19.50F, kStairApproachTop + 0.97F, -120.68F}, {-15.43F, kStairApproachTop + 1.05F, -120.60F},
                         Material::Yellow));
    for (const float z : {-123.90F, -122.30F}) {
        frame.push_back(span({-19.50F, kStairApproachTop, z - 0.04F}, {-19.42F, kStairApproachTop + 1.05F, z + 0.04F},
                             Material::Yellow));
    }
    frame.push_back(span({-19.50F, kStairApproachTop + 0.97F, -124.05F}, {-19.42F, kStairApproachTop + 1.05F, -120.60F},
                         Material::Yellow));
    // The catch lever's post, and the chain's guide on a bracket off it.
    frame.push_back(span({-16.10F, kStairApproachTop, -120.80F}, {-15.90F, 36.10F, -120.60F}, Material::Rust));
    frame.push_back(span({-17.55F, 35.05F, -120.80F}, {-16.10F, 35.15F, -120.70F}, Material::Rust));
    frame.push_back(span({-17.55F, 35.05F, -120.95F}, {-17.45F, 35.15F, -120.80F}, Material::Hazard));
    // Broad side exit, clear of the stair's swept flight and its rail.
    frame.push_back(span({-3.30F,43.88F,-124.05F},{-0.30F,44.0F,-123.14F},Material::Galvanised));
    // Open receiver frame. Two crush stacks straddle the retained central
    // blade; the blade passes through the gap without contact.
    frame.push_back(span({-3.40F,41.10F,-123.00F},{-0.60F,41.40F,-121.20F},Material::Steel));
    for (float x : {-3.10F,-0.90F}) {
        frame.push_back(span({x-.15F,41.10F,-123.85F},{x+.15F,44.0F,-123.55F},Material::Rust));
        frame.push_back(span({x-.15F,41.10F,-123.85F},{x+.15F,41.40F,-121.20F},Material::Rust));
    }
}
