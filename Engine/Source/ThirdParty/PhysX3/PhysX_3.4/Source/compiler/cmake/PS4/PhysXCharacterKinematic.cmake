#
# Build PhysXCharacterKinematic
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXCharacterKinematic/src)


SET(PHYSXCHARACTERKINEMATICS_COMPILE_DEFS 

	# Common to all configurations

	${PHYSX_PS4_COMPILE_DEFS}

	$<$<CONFIG:debug>:${PHYSX_PS4_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_PS4_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_PS4_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_PS4_RELEASE_COMPILE_DEFS};>
)

# include common PhysXCharacterKinematic settings
INCLUDE(../common/PhysXCharacterKinematic.cmake)

