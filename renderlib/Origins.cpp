#include "Origins.h"

#include "AppScene.h"
#include "Logging.h"
#include "RenderSettings.h"

void
Origins::update(SceneView& scene)
{
  if (scene.scene) {

    // e.g. find all selected objects in scene and collect up their centers/transforms here.
    SceneObject* obj = scene.selection;

    // save the initial transform? we could use this to reset things if cancelled.

    // this is a copy!!!
    m_origins = { obj->m_transform };

    // arealight special case:
    // we want the rotate manipulator to be centered at the target of the light.
    // This could be a virtual SceneObject method like getManipulatorCenter()
    Light& lt = scene.scene->AreaLight();
    m_origins[0].m_center = lt.m_Target;
  }
}

void
Origins::translate(SceneView& scene, glm::vec3 motion)
{
  // e.g. find all selected objects in scene and apply transforms here.

  glm::vec3 p = m_origins[0].m_center + motion;

  SceneObject* lt = scene.selection;
  // the above line could be more like:
  // SceneObject& obj = scene.getSelection();

  // actually set the light's transform here!!
  lt->m_transform.m_center = p;

  lt->updateTransform();
  scene.renderSettings->m_DirtyFlags.SetFlag(LightsDirty);
}

void
Origins::rotate(SceneView& scene, glm::quat rotation)
{
  // e.g. find all selected objects in scene and apply transforms here.

  // apply the rotation to the scene's selection
  // but do not bake it in yet?
  // if "cancelled" we could always restore the original rotation.
  glm::quat q = rotation * m_origins[0].m_rotation;

  SceneObject* lt = scene.selection;
  if (!lt) {
    return;
  }

  // actually set the light's transform here!!
  lt->m_transform.m_rotation = q;

  lt->updateTransform();

  // special for light!!!
  scene.renderSettings->m_DirtyFlags.SetFlag(LightsDirty);
}
