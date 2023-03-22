#include "ViewerState.h"

#include "command.h"

#include "renderlib/AppScene.h"
#include "renderlib/CCamera.h"
#include "renderlib/GradientData.h"
#include "renderlib/Logging.h"
#include "renderlib/version.h"
#include "renderlib/version.hpp"

#include <QFile>
#include <QFileInfo>

#include <sstream>

template<typename K, typename V>
std::unordered_map<V, K>
inverse_map(std::unordered_map<K, V>& map)
{
  std::unordered_map<V, K> inv;
  std::for_each(
    map.begin(), map.end(), [&inv](const std::pair<K, V>& p) { inv.insert(std::make_pair(p.second, p.first)); });
  return inv;
}

std::unordered_map<GradientEditMode, Serialize::GradientEditMode_PID> g_GradientModeToPermId = {
  { GradientEditMode::WINDOW_LEVEL, Serialize::GradientEditMode_PID::WINDOW_LEVEL },
  { GradientEditMode::ISOVALUE, Serialize::GradientEditMode_PID::ISOVALUE },
  { GradientEditMode::PERCENTILE, Serialize::GradientEditMode_PID::PERCENTILE },
  { GradientEditMode::CUSTOM, Serialize::GradientEditMode_PID::CUSTOM }
};
auto g_PermIdToGradientMode = inverse_map(g_GradientModeToPermId);

std::unordered_map<eRenderDurationType, Serialize::DurationType_PID> g_RenderDurationTypeToPermId = {
  { eRenderDurationType::SAMPLES, Serialize::DurationType_PID::SAMPLES },
  { eRenderDurationType::TIME, Serialize::DurationType_PID::TIME },
};
auto g_PermIdToRenderDurationType = inverse_map(g_RenderDurationTypeToPermId);

std::unordered_map<ProjectionMode, Serialize::Projection_PID> g_ProjectionToPermId = {
  { ProjectionMode::PERSPECTIVE, Serialize::Projection_PID::PERSPECTIVE },
  { ProjectionMode::ORTHOGRAPHIC, Serialize::Projection_PID::ORTHOGRAPHIC },
};
auto g_PermIdToProjection = inverse_map(g_ProjectionToPermId);

std::unordered_map<int, Serialize::LightType> g_LightTypeToPermId = {
  { 1, Serialize::LightType::SKY },
  { 0, Serialize::LightType::AREA },
};
auto g_PermIdToLightType = inverse_map(g_LightTypeToPermId);

Serialize::ViewerState
stateFromJson(const nlohmann::json& jsonDoc)
{
  // VERSION MUST EXIST.  THROW OR PANIC IF NOT.
  std::array<int, 3> v = { 0, 0, 0 };
  if (jsonDoc.contains("version")) {
    auto ja = jsonDoc["version"];
    v[0] = ja.at(0).get<int32_t>();
    v[1] = ja.at(1).get<int32_t>();
    v[2] = ja.at(2).get<int32_t>();
  } else {
    // ERROR
  }

  Version version(v);

  // we will fill this in from the jsonDoc.
  Serialize::ViewerState stateV2;

  // version checks.  Parse old data structures here.
  if (version <= Version(1, 4, 1)) {
    Serialize::ViewerState_V1 stateV1 = jsonDoc.get<Serialize::ViewerState_V1>();
    // fill in this from the old data structure.
    stateV2 = fromV1(stateV1);
  } else {
    stateV2 = jsonDoc.get<Serialize::ViewerState>();
  }
  return stateV2;
}

QString
stateToPythonScript(const Serialize::ViewerState& s)
{
  QFileInfo fi(QString::fromStdString(s.datasets[0].url));
  QString outFileName = fi.baseName();

  std::ostringstream ss;
  ss << "# pip install agave_pyclient" << std::endl;
  ss << "# agave --server &" << std::endl;
  ss << "# python myscript.py" << std::endl << std::endl;
  ss << "import agave_pyclient as agave" << std::endl << std::endl;
  ss << "r = agave.AgaveRenderer()" << std::endl;
  std::string obj = "r.";
  ss << obj
     << LoadVolumeFromFileCommand({ s.datasets[0].url, (int32_t)s.datasets[0].scene, (int32_t)s.datasets[0].time })
          .toPythonString()
     << std::endl;
  // TODO use window size or render window capture dims?
  ss << obj << SetResolutionCommand({ s.capture.width, s.capture.height }).toPythonString() << std::endl;
  ss << obj
     << SetBackgroundColorCommand({ s.backgroundColor[0], s.backgroundColor[1], s.backgroundColor[2] }).toPythonString()
     << std::endl;
  ss << obj << ShowBoundingBoxCommand({ s.showBoundingBox }).toPythonString() << std::endl;
  ss << obj
     << SetBoundingBoxColorCommand({ s.boundingBoxColor[0], s.boundingBoxColor[1], s.boundingBoxColor[2] })
          .toPythonString()
     << std::endl;
  // TODO use value from viewport or render window capture settings?
  ss << obj << SetRenderIterationsCommand({ s.capture.samples }).toPythonString() << std::endl;
  ss << obj << SetPrimaryRayStepSizeCommand({ s.pathTracer.primaryStepSize }).toPythonString() << std::endl;
  ss << obj << SetSecondaryRayStepSizeCommand({ s.pathTracer.secondaryStepSize }).toPythonString() << std::endl;
  ss << obj << SetVoxelScaleCommand({ s.scale[0], s.scale[1], s.scale[2] }).toPythonString() << std::endl;
  ss << obj
     << SetClipRegionCommand({ s.clipRegion[0][0],
                               s.clipRegion[0][1],
                               s.clipRegion[1][0],
                               s.clipRegion[1][1],
                               s.clipRegion[2][0],
                               s.clipRegion[2][1] })
          .toPythonString()
     << std::endl;
  ss << obj << SetCameraPosCommand({ s.camera.eye[0], s.camera.eye[1], s.camera.eye[2] }).toPythonString() << std::endl;
  ss << obj << SetCameraTargetCommand({ s.camera.target[0], s.camera.target[1], s.camera.target[2] }).toPythonString()
     << std::endl;
  ss << obj << SetCameraUpCommand({ s.camera.up[0], s.camera.up[1], s.camera.up[2] }).toPythonString() << std::endl;
  ss << obj
     << SetCameraProjectionCommand(
          { g_PermIdToProjection[s.camera.projection],
            s.camera.projection == Serialize::Projection_PID::PERSPECTIVE ? s.camera.fovY : s.camera.orthoScale })
          .toPythonString()
     << std::endl;

  ss << obj << SetCameraExposureCommand({ s.camera.exposure }).toPythonString() << std::endl;
  ss << obj << SetDensityCommand({ s.density }).toPythonString() << std::endl;
  ss << obj << SetCameraApertureCommand({ s.camera.aperture }).toPythonString() << std::endl;
  ss << obj << SetCameraFocalDistanceCommand({ s.camera.focalDistance }).toPythonString() << std::endl;

  // per-channel
  for (std::int32_t i = 0; i < s.channels.size(); ++i) {
    const Serialize::ChannelSettings_V1& ch = s.channels[i];
    ss << obj << EnableChannelCommand({ i, ch.enabled ? 1 : 0 }).toPythonString() << std::endl;
    ss << obj
       << SetDiffuseColorCommand({ i, ch.diffuseColor[0], ch.diffuseColor[1], ch.diffuseColor[2], 1.0f })
            .toPythonString()
       << std::endl;
    ss << obj
       << SetSpecularColorCommand({ i, ch.specularColor[0], ch.specularColor[1], ch.specularColor[2], 0.0f })
            .toPythonString()
       << std::endl;
    ss << obj
       << SetEmissiveColorCommand({ i, ch.emissiveColor[0], ch.emissiveColor[1], ch.emissiveColor[2], 0.0f })
            .toPythonString()
       << std::endl;
    ss << obj << SetGlossinessCommand({ i, ch.glossiness }).toPythonString() << std::endl;
    ss << obj << SetOpacityCommand({ i, ch.opacity }).toPythonString() << std::endl;
    // depending on current mode:
    switch (g_PermIdToGradientMode[ch.lutParams.mode]) {
      case GradientEditMode::WINDOW_LEVEL:
        ss << obj << SetWindowLevelCommand({ i, ch.lutParams.window, ch.lutParams.level }).toPythonString()
           << std::endl;
        break;
      case GradientEditMode::ISOVALUE:
        ss << obj << SetIsovalueThresholdCommand({ i, ch.lutParams.isovalue, ch.lutParams.isorange }).toPythonString()
           << std::endl;
        break;
      case GradientEditMode::PERCENTILE:
        ss << obj << SetPercentileThresholdCommand({ i, ch.lutParams.pctLow, ch.lutParams.pctHigh }).toPythonString()
           << std::endl;
        break;
      case GradientEditMode::CUSTOM:
        std::vector<float> v;
        for (auto p : ch.lutParams.controlPoints) {
          v.push_back(p.x);
          v.push_back(p.value[0]);
          v.push_back(p.value[1]);
          v.push_back(p.value[2]);
          v.push_back(p.value[3]);
        }
        ss << obj << SetControlPointsCommand({ i, v }).toPythonString() << std::endl;
        break;
    }
  }

  // lighting

  // TODO get sky light.
  // assuming this is light 0 for now.
  const Serialize::LightSettings_V1& light0 = s.lights[0];
  const Serialize::LightSettings_V1& light1 = s.lights[1];
  ss << obj
     << SetSkylightTopColorCommand({ light0.topColor[0] * light0.topColorIntensity,
                                     light0.topColor[1] * light0.topColorIntensity,
                                     light0.topColor[2] * light0.topColorIntensity })
          .toPythonString()
     << std::endl;
  ss << obj
     << SetSkylightMiddleColorCommand({ light0.middleColor[0] * light0.middleColorIntensity,
                                        light0.middleColor[1] * light0.middleColorIntensity,
                                        light0.middleColor[2] * light0.middleColorIntensity })
          .toPythonString()
     << std::endl;
  ss << obj
     << SetSkylightBottomColorCommand({ light0.bottomColor[0] * light0.bottomColorIntensity,
                                        light0.bottomColor[1] * light0.bottomColorIntensity,
                                        light0.bottomColor[2] * light0.bottomColorIntensity })
          .toPythonString()
     << std::endl;
  ss << obj << SetLightPosCommand({ 0, light1.distance, light1.theta, light1.phi }).toPythonString() << std::endl;
  ss << obj
     << SetLightColorCommand({ 0,
                               light1.color[0] * light1.colorIntensity,
                               light1.color[1] * light1.colorIntensity,
                               light1.color[2] * light1.colorIntensity })
          .toPythonString()
     << std::endl;
  ss << obj << SetLightSizeCommand({ 0, light1.width, light1.height }).toPythonString() << std::endl;

  ss << obj << SessionCommand({ outFileName.toStdString() + ".png" }).toPythonString() << std::endl;
  ss << obj << RequestRedrawCommand({}).toPythonString() << std::endl;
  std::string sout(ss.str());
  // LOG_DEBUG << s;
  return QString::fromStdString(sout);
}

LoadSpec
stateToLoadSpec(const Serialize::ViewerState& state)
{
  const Serialize::LoadSettings& s = state.datasets[0];
  LoadSpec spec;
  spec.filepath = s.url;
  spec.subpath = s.subpath;
  spec.scene = s.scene;
  spec.time = s.time;
  spec.channels = s.channels;
  spec.minx = s.clipRegion[0][0];
  spec.maxx = s.clipRegion[0][1];
  spec.miny = s.clipRegion[1][0];
  spec.maxy = s.clipRegion[1][1];
  spec.minz = s.clipRegion[2][0];
  spec.maxz = s.clipRegion[2][1];
  return spec;
}

GradientData
stateToGradientData(const Serialize::ViewerState& state, int channelIndex)
{
  GradientData gd;
  const auto& ch = state.channels[channelIndex];
  const auto& lut = ch.lutParams;
  gd.m_activeMode = g_PermIdToGradientMode[lut.mode];
  gd.m_window = lut.window;
  gd.m_level = lut.level;
  gd.m_isovalue = lut.isovalue;
  gd.m_isorange = lut.isorange;
  gd.m_pctLow = lut.pctLow;
  gd.m_pctHigh = lut.pctHigh;
  for (size_t i = 0; i < lut.controlPoints.size(); i += 5) {
    LutControlPoint cp;
    cp.first = lut.controlPoints[i].x;
    // use first value?  or "alpha" (4th) value?
    cp.second = lut.controlPoints[i].value[0];
    gd.m_customControlPoints.push_back(cp);
  }
  return gd;
}

Light
stateToLight(const Serialize::ViewerState& state, int lightIndex)
{
  Light lt;
  const Serialize::LightSettings_V1& l = state.lights[lightIndex];
  lt.m_T = g_PermIdToLightType[l.type];
  lt.m_Distance = l.distance;
  lt.m_Theta = l.theta;
  lt.m_Phi = l.phi;
  lt.m_ColorTop = glm::vec3(l.topColor[0], l.topColor[1], l.topColor[2]);
  lt.m_ColorMiddle = glm::vec3(l.middleColor[0], l.middleColor[1], l.middleColor[2]);
  lt.m_ColorBottom = glm::vec3(l.bottomColor[0], l.bottomColor[1], l.bottomColor[2]);
  lt.m_Color = glm::vec3(l.color[0], l.color[1], l.color[2]);
  lt.m_ColorTopIntensity = l.topColorIntensity;
  lt.m_ColorMiddleIntensity = l.middleColorIntensity;
  lt.m_ColorBottomIntensity = l.bottomColorIntensity;
  lt.m_ColorIntensity = l.colorIntensity;
  lt.m_Width = l.width;
  lt.m_Height = l.height;

  return lt;
}
Serialize::LoadSettings
fromLoadSpec(const LoadSpec& loadSpec)
{
  Serialize::LoadSettings s;
  s.url = loadSpec.filepath;
  s.subpath = loadSpec.subpath;
  s.scene = loadSpec.scene;
  s.time = loadSpec.time;
  s.channels = loadSpec.channels;
  s.clipRegion[0][0] = loadSpec.minx;
  s.clipRegion[0][1] = loadSpec.maxx;
  s.clipRegion[1][0] = loadSpec.miny;
  s.clipRegion[1][1] = loadSpec.maxy;
  s.clipRegion[2][0] = loadSpec.minz;
  s.clipRegion[2][1] = loadSpec.maxz;
  return s;
}

Serialize::LightSettings_V1
fromLight(const Light& lt)
{
  Serialize::LightSettings_V1 s;
  s.type = g_LightTypeToPermId[lt.m_T];
  s.distance = lt.m_Distance;
  s.theta = lt.m_Theta;
  s.phi = lt.m_Phi;
  s.topColor = { lt.m_ColorTop.r, lt.m_ColorTop.g, lt.m_ColorTop.b };
  s.middleColor = { lt.m_ColorMiddle.r, lt.m_ColorMiddle.g, lt.m_ColorMiddle.b };
  s.color = { lt.m_Color.r, lt.m_Color.g, lt.m_Color.b };
  s.bottomColor = { lt.m_ColorBottom.r, lt.m_ColorBottom.g, lt.m_ColorBottom.b };
  s.topColorIntensity = lt.m_ColorTopIntensity;
  s.middleColorIntensity = lt.m_ColorMiddleIntensity;
  s.colorIntensity = lt.m_ColorIntensity;
  s.bottomColorIntensity = lt.m_ColorBottomIntensity;
  s.width = lt.m_Width;
  s.height = lt.m_Height;
  return s;
}

Serialize::CaptureSettings
fromCaptureSettings(const CaptureSettings& cs)
{
  Serialize::CaptureSettings s;
  // TODO make sure capture settings are initialized to window size until
  // render dialog has been opened.
  // v.m_resolutionX = m_glView->size().width();
  // v.m_resolutionY = m_glView->size().height();
  s.width = cs.width;
  s.height = cs.height;
  s.samples = cs.samples;
  s.seconds = cs.duration;
  s.durationType = g_RenderDurationTypeToPermId[cs.durationType];
  s.startTime = cs.startTime;
  s.endTime = cs.endTime;
  s.outputDirectory = cs.outputDir;
  s.filenamePrefix = cs.filenamePrefix;
  return s;
}

Serialize::LutParams_V1
fromGradientData(const GradientData& gd)
{
  Serialize::LutParams_V1 s;
  s.mode = g_GradientModeToPermId[gd.m_activeMode];
  s.window = gd.m_window;
  s.level = gd.m_level;
  s.isovalue = gd.m_isovalue;
  s.isorange = gd.m_isorange;
  s.pctLow = gd.m_pctLow;
  s.pctHigh = gd.m_pctHigh;
  for (const auto& cp : gd.m_customControlPoints) {
    Serialize::ControlPointSettings_V1 c;
    c.x = cp.first;
    c.value = { cp.second, cp.second, cp.second, 1.0 };
    s.controlPoints.push_back(c);
  }
  return s;
}