# FootSyncMarkerGenerator

Unreal Engine 5 plugin for automatic foot contact detection and sync marker generation in locomotion animations.

## Features

- **Automatic Foot Contact Detection**: Analyzes animation sequences to detect when feet make contact with the ground
- **Multiple Detection Algorithms**:
  - **PelvisCrossing**: Detects when foot crosses the pelvis position along the movement axis
  - **VelocityCurve**: Finds velocity minima where foot movement stops
  - **Saliency**: Uses trajectory curvature analysis based on IEEE paper "Foot plant detection by curve saliency"
  - **Composite**: Combines multiple algorithms with weighted voting
- **Sync Marker Generation**: Automatically adds animation sync markers at detected contact points
- **Optional Curve Generation**: Creates distance and velocity curves for debugging/visualization
- **Per-Animation Overrides**: Override detection parameters for individual animations

## Requirements

- Unreal Engine 5.7+
- AnimationModifiers plugin (included with UE5)

## Installation

1. Clone this repository into your project's `Plugins/` directory
2. Enable the plugin in your `.uproject` file:
```json
{
  "Plugins": [
    {
      "Name": "FootSyncMarkerGenerator",
      "Enabled": true
    }
  ]
}
```
3. Regenerate project files and build

## Usage

### Basic Setup

1. Open Project Settings → Foot Sync Marker Settings
2. Configure bone name patterns for your skeleton:
   - Pelvis bone pattern (e.g., `pelvis`, `Hips`)
   - Left/Right foot patterns (e.g., `foot_l`, `LeftFoot`)
3. Select detection method (Composite recommended)

### Applying to Animations

1. Open an Animation Sequence
2. Right-click → Apply Animation Modifier
3. Select `FootSyncMarkerModifier`
4. Configure locomotion type (Biped/Quadruped/Custom)
5. Apply the modifier

### Settings

| Setting | Description | Default |
|---------|-------------|---------|
| DetectionMethod | Algorithm to use | Composite |
| MaxMarkersPerFoot | Maximum sync markers per foot | 2 |
| MinimumConfidence | Confidence threshold for markers | 0.3 |
| VelocityMinimumThreshold | Velocity threshold (cm/s) | 5.0 |
| SaliencyThreshold | Saliency detection threshold | 0.5 |
| bGuaranteeMinimumOne | Always generate at least one marker | true |

### Per-Animation Overrides

The modifier supports overriding these settings per-animation:
- `bOverrideMaxMarkersPerFoot` / `MaxMarkersPerFootOverride`
- `bOverrideMinimumConfidence` / `MinimumConfidenceOverride`
- `bOverrideVelocityThreshold` / `VelocityThresholdOverride`
- `bOverrideSaliencyThreshold` / `SaliencyThresholdOverride`
- `bOverrideGuaranteeMinimumOne` / `bGuaranteeMinimumOneOverride`

## Architecture

```
FootSyncMarkerGenerator/
├── Source/
│   └── FootSyncMarkerGenerator/
│       ├── Public/
│       │   ├── FootSyncMarkerModifier.h    # Main animation modifier
│       │   ├── FootSyncMarkerSettings.h    # Project settings
│       │   ├── LocomotionPresets.h         # Foot/preset definitions
│       │   └── Detection/
│       │       ├── IFootContactDetector.h      # Detector interface
│       │       ├── PelvisCrossingDetector.h    # Pelvis-based detection
│       │       ├── VelocityCurveDetector.h     # Velocity-based detection
│       │       ├── SaliencyDetector.h          # Curvature-based detection
│       │       └── CompositeDetector.h         # Multi-algorithm fusion
│       └── Private/
│           └── ...
└── FootSyncMarkerGenerator.uplugin
```

## License

MIT License - see [LICENSE](LICENSE) for details.
