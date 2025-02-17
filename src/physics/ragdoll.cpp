#include "pch.h"
#include "ragdoll.h"

#ifndef PHYSICS_ONLY
#include "rendering/pbr.h"
#include "geometry/mesh.h"
#include "geometry/mesh_builder.h"
#endif

void humanoid_ragdoll::initialize(game_scene& scene, vec3 initialHipPosition, float initialRotation)
{
	float scale = 0.42f; // This file is completely hardcoded. I initially screwed up the scaling a bit, so this factor brings everything to the correct scale (and therefore weight).

	bool ragdollKinematic = false;
	float ragdollGravityFactor = 1.f;
	float ragdollDensity = 985.f; // Average density of human body in kg/m3.
	float ragdollFriction = 1.f;

	physics_material material = { physics_material_type_flesh, 0.2f, ragdollFriction, ragdollDensity };

	trs torsoTransform(scale * vec3(0.f, 0.f, 0.f), quat::identity);
	trs headTransform(scale * vec3(0.f, 1.45f, 0.f), quat::identity);
	trs leftUpperArmTransform(scale * vec3(-0.6f, 0.75f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-30.f)));
	trs leftLowerArmTransform(scale * vec3(-0.884f, 0.044f, -0.043f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-20.f)));
	trs rightUpperArmTransform(scale * vec3(0.6f, 0.75f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(30.f)));
	trs rightLowerArmTransform(scale * vec3(0.884f, 0.044f, -0.043f), quat(vec3(0.f, 0.f, 1.f), deg2rad(20.f)));
	trs leftUpperLegTransform(scale * vec3(-0.371f, -0.812f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-10.f)));
	trs leftLowerLegTransform(scale * vec3(-0.452f, -1.955f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-3.5f)));
	trs leftFootTransform(scale * vec3(-0.498f, -2.585f, -0.18f), quat::identity);
	trs leftToesTransform(scale * vec3(-0.498f, -2.585f, -0.637f), quat::identity);
	trs rightUpperLegTransform(scale * vec3(0.371f, -0.812f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(10.f)));
	trs rightLowerLegTransform(scale * vec3(0.452f, -1.955f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(3.5f)));
	trs rightFootTransform(scale * vec3(0.498f, -2.585f, -0.18f), quat::identity);
	trs rightToesTransform(scale * vec3(0.498f, -2.585f, -0.637f), quat::identity);

	torso = scene.createEntity("Torso")
		.addComponent<transform_component>(torsoTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(-0.2f, 0.f, 0.f),    scale *  vec3(0.2f, 0.f, 0.f),    scale * 0.25f }, material))
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(-0.16f, 0.32f, 0.f), scale *  vec3(0.16f, 0.32f, 0.f), scale * 0.2f }, material))
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(-0.14f, 0.62f, 0.f), scale *  vec3(0.14f, 0.62f, 0.f), scale * 0.22f }, material))
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(-0.14f, 0.92f, 0.f), scale *  vec3(0.14f, 0.92f, 0.f), scale * 0.2f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	head = scene.createEntity("Head")
		.addComponent<transform_component>(headTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.075f, 0.f), scale * vec3(0.f, 0.075f, 0.f), scale * 0.25f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftUpperArm = scene.createEntity("Left upper arm")
		.addComponent<transform_component>(leftUpperArmTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftLowerArm = scene.createEntity("Left lower arm")
		.addComponent<transform_component>(leftLowerArmTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightUpperArm = scene.createEntity("Right upper arm")
		.addComponent<transform_component>(rightUpperArmTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightLowerArm = scene.createEntity("Right lower arm")
		.addComponent<transform_component>(rightLowerArmTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftUpperLeg = scene.createEntity("Left upper leg")
		.addComponent<transform_component>(leftUpperLegTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.25f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftLowerLeg = scene.createEntity("Left lower leg")
		.addComponent<transform_component>(leftLowerLegTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.18f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftFoot = scene.createEntity("Left foot")
		.addComponent<transform_component>(leftFootTransform)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(scale * vec3(0.f), scale * vec3(0.1587f, 0.1f, 0.3424f)), material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftToes = scene.createEntity("Left toes")
		.addComponent<transform_component>(leftToesTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(-0.0587f, 0.f, 0.f), scale * vec3(0.0587f, 0.f, 0.f), scale * 0.1f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightUpperLeg = scene.createEntity("Right upper leg")
		.addComponent<transform_component>(rightUpperLegTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.25f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightLowerLeg = scene.createEntity("Right lower leg")
		.addComponent<transform_component>(rightLowerLegTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.18f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightFoot = scene.createEntity("Right foot")
		.addComponent<transform_component>(rightFootTransform)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(scale * vec3(0.f), scale * vec3(0.1587f, 0.1f, 0.3424f)), material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightToes = scene.createEntity("Right toes")
		.addComponent<transform_component>(rightToesTransform)
		.addComponent<collider_component>(collider_component::asCapsule({ scale * vec3(-0.0587f, 0.f, 0.f), scale * vec3(0.0587f, 0.f, 0.f), scale * 0.1f }, material))
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	neckConstraint = addConeTwistConstraintFromGlobalPoints(torso, head, transformPosition(torsoTransform, scale * vec3(0.f, 1.2f, 0.f)), vec3(0.f, 1.f, 0.f), deg2rad(50.f), deg2rad(90.f));
	leftShoulderConstraint = addConeTwistConstraintFromGlobalPoints(torso, leftUpperArm, transformPosition(torsoTransform, scale * vec3(-0.4f, 1.f, 0.f)), vec3(-1.f, 0.f, 0.f), deg2rad(130.f), deg2rad(90.f));
	leftElbowConstraint = addHingeConstraintFromGlobalPoints(leftUpperArm, leftLowerArm, transformPosition(leftUpperArmTransform, scale * vec3(0.f, -0.42f, 0.f)), normalize(vec3(1.f, 0.f, 1.f)), deg2rad(-5.f), deg2rad(85.f));
	rightShoulderConstraint = addConeTwistConstraintFromGlobalPoints(torso, rightUpperArm, transformPosition(torsoTransform, scale * vec3(0.4f, 1.f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(130.f), deg2rad(90.f));
	rightElbowConstraint = addHingeConstraintFromGlobalPoints(rightUpperArm, rightLowerArm, transformPosition(rightUpperArmTransform, scale * vec3(0.f, -0.42f, 0.f)), normalize(vec3(1.f, 0.f, -1.f)), deg2rad(-5.f), deg2rad(85.f));
	
	leftHipConstraint = addConeTwistConstraintFromGlobalPoints(torso, leftUpperLeg, transformPosition(torsoTransform, scale * vec3(-0.3f, -0.25f, 0.f)), transformDirection(leftUpperLegTransform, vec3(0.f, -1.f, 0.f)), -1.f, deg2rad(30.f));
	leftKneeConstraint = addHingeConstraintFromGlobalPoints(leftUpperLeg, leftLowerLeg, transformPosition(leftUpperLegTransform, scale * vec3(0.f, -0.6f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(-90.f), deg2rad(5.f));
	leftAnkleConstraint = addConeTwistConstraintFromGlobalPoints(leftLowerLeg, leftFoot, transformPosition(leftLowerLegTransform, scale * vec3(0.f, -0.52f, 0.f)), transformDirection(leftLowerLegTransform, vec3(0.f, -1.f, 0.f)), deg2rad(75.f), deg2rad(20.f));
	leftToesConstraint = addHingeConstraintFromGlobalPoints(leftFoot, leftToes, transformPosition(leftFootTransform, scale * vec3(0.f, 0.f, -0.36f)), vec3(1.f, 0.f, 0.f), deg2rad(-45.f), deg2rad(45.f));

	rightHipConstraint = addConeTwistConstraintFromGlobalPoints(torso, rightUpperLeg, transformPosition(torsoTransform, scale * vec3(0.3f, -0.25f, 0.f)), transformDirection(rightUpperLegTransform, vec3(0.f, -1.f, 0.f)), -1.f, deg2rad(30.f));
	rightKneeConstraint = addHingeConstraintFromGlobalPoints(rightUpperLeg, rightLowerLeg, transformPosition(rightUpperLegTransform, scale * vec3(0.f, -0.6f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(-90.f), deg2rad(5.f));
	rightAnkleConstraint = addConeTwistConstraintFromGlobalPoints(rightLowerLeg, rightFoot, transformPosition(rightLowerLegTransform, scale * vec3(0.f, -0.52f, 0.f)), transformDirection(rightLowerLegTransform, vec3(0.f, -1.f, 0.f)), deg2rad(75.f), deg2rad(20.f));
	rightToesConstraint = addHingeConstraintFromGlobalPoints(rightFoot, rightToes, transformPosition(rightFootTransform, scale * vec3(0.f, 0.f, -0.36f)), vec3(1.f, 0.f, 0.f), deg2rad(-45.f), deg2rad(45.f));


	quat rotation(vec3(0.f, 1.f, 0.f), initialRotation);
	for (uint32 i = 0; i < arraysize(bodyParts); ++i)
	{
		transform_component& transform = bodyParts[i].getComponent<transform_component>();
		transform.rotation = rotation * transform.rotation;
		transform.position = rotation * transform.position + initialHipPosition;

		bodyParts[i].getComponent<physics_transform0_component>() = bodyParts[i].getComponent<physics_transform1_component>() = transform;
	}

#if 0
	float totalMass = 1.f / torso.getComponent<rigid_body_component>().invMass +
		1.f / head.getComponent<rigid_body_component>().invMass +
		1.f / leftUpperArm.getComponent<rigid_body_component>().invMass +
		1.f / leftLowerArm.getComponent<rigid_body_component>().invMass +
		1.f / rightUpperArm.getComponent<rigid_body_component>().invMass +
		1.f / rightLowerArm.getComponent<rigid_body_component>().invMass +
		1.f / leftUpperLeg.getComponent<rigid_body_component>().invMass +
		1.f / leftLowerLeg.getComponent<rigid_body_component>().invMass +
		1.f / leftFoot.getComponent<rigid_body_component>().invMass +
		1.f / leftToes.getComponent<rigid_body_component>().invMass +
		1.f / rightUpperLeg.getComponent<rigid_body_component>().invMass +
		1.f / rightLowerLeg.getComponent<rigid_body_component>().invMass +
		1.f / rightFoot.getComponent<rigid_body_component>().invMass +
		1.f / rightToes.getComponent<rigid_body_component>().invMass;

	std::cout << totalMass << '\n';
#endif


	torsoParent = {};
	headParent = torso;
	leftUpperArmParent = torso;
	leftLowerArmParent = leftUpperArm;
	rightUpperArmParent = torso;
	rightLowerArmParent = rightUpperArm;
	leftUpperLegParent = torso;
	leftLowerLegParent = leftUpperLeg;
	leftFootParent = leftLowerLeg;
	leftToesParent = leftFoot;
	rightUpperLegParent = torso;
	rightLowerLegParent = rightUpperLeg;
	rightFootParent = rightLowerLeg;
	rightToesParent = rightFoot;


	// Graphics.


#ifndef PHYSICS_ONLY

	mesh_builder builder(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	auto ragdollMaterial = createPBRMaterial(
		{}, {}, {}, {}, vec4(0.f), vec4(161.f, 102.f, 94.f, 255.f) / 255.f, 1.f, 0.f);
	//auto ragdollMaterial = lollipopMaterial;

	auto ragdollTorsoMesh = make_ref<multi_mesh>();
	{
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(-0.2f, 0.f, 0.f), scale * vec3(0.2f, 0.f, 0.f), scale * 0.25f));
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(-0.16f, 0.32f, 0.f), scale * vec3(0.16f, 0.32f, 0.f), scale * 0.2f));
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(-0.14f, 0.62f, 0.f), scale * vec3(0.14f, 0.62f, 0.f), scale * 0.22f));
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(-0.14f, 0.92f, 0.f), scale * vec3(0.14f, 0.92f, 0.f), scale * 0.2f));
		ragdollTorsoMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}
	
	auto ragdollHeadMesh = make_ref<multi_mesh>();
	{
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(0.f, -0.075f, 0.f), scale * vec3(0.f, 0.075f, 0.f), scale * 0.25f));
		ragdollHeadMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}

	auto ragdollArmMesh = make_ref<multi_mesh>();
	{
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f));
		ragdollArmMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}

	auto ragdollUpperLegMesh = make_ref<multi_mesh>();
	{
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.25f));
		ragdollUpperLegMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}

	auto ragdollLowerLegMesh = make_ref<multi_mesh>();
	{
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.18f));
		ragdollLowerLegMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}

	auto ragdollFootMesh = make_ref<multi_mesh>();
	{
		builder.pushBox(box_mesh_desc{ vec3(0.f), scale * vec3(0.1587f, 0.1f, 0.3424f) });
		ragdollFootMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}

	auto ragdollToesMesh = make_ref<multi_mesh>();
	{
		builder.pushCapsule(capsule_mesh_desc(scale * vec3(-0.0587f, 0.f, 0.f), scale * vec3(0.0587f, 0.f, 0.f), scale * 0.1f));
		ragdollToesMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, ragdollMaterial });
	}

	torso.addComponent<raster_component>(ragdollTorsoMesh);
	head.addComponent<raster_component>(ragdollHeadMesh);
	leftUpperArm.addComponent<raster_component>(ragdollArmMesh);
	leftLowerArm.addComponent<raster_component>(ragdollArmMesh);
	rightUpperArm.addComponent<raster_component>(ragdollArmMesh);
	rightLowerArm.addComponent<raster_component>(ragdollArmMesh);
	leftUpperLeg.addComponent<raster_component>(ragdollUpperLegMesh);
	leftLowerLeg.addComponent<raster_component>(ragdollLowerLegMesh);
	leftFoot.addComponent<raster_component>(ragdollFootMesh);
	leftToes.addComponent<raster_component>(ragdollToesMesh);
	rightUpperLeg.addComponent<raster_component>(ragdollUpperLegMesh);
	rightLowerLeg.addComponent<raster_component>(ragdollLowerLegMesh);
	rightFoot.addComponent<raster_component>(ragdollFootMesh);
	rightToes.addComponent<raster_component>(ragdollToesMesh);

	ragdollTorsoMesh->mesh =
		ragdollHeadMesh->mesh =
		ragdollArmMesh->mesh =
		ragdollUpperLegMesh->mesh =
		ragdollLowerLegMesh->mesh =
		ragdollFootMesh->mesh =
		ragdollToesMesh->mesh =
		builder.createDXMesh();

#endif
}

humanoid_ragdoll humanoid_ragdoll::create(game_scene& scene, vec3 initialHipPosition, float initialRotation)
{
	humanoid_ragdoll ragdoll;
	ragdoll.initialize(scene, initialHipPosition, initialRotation);
	return ragdoll;
}
