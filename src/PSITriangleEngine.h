// PSIEngine Copyright (c) 2021 Sakari Lehtonen <sakari@psitriangle.net>
//
// Includes all the PSITriangleEngine classes.

#pragma once

#define PSI_AUDIO_SUPPORT true

#include "PSIGlobals.h"

#include "PSIVideo.h"
#ifdef PSI_AUDIO_SUPPORT
#include "PSIAudio.h"
#endif

#include "PSIGLUtils.h"
#include "PSIGLRenderer.h"
#include "PSIGLTexture.h"
#include "PSIAABB.h"
#include "PSICamera.h"

#include "PSIResourceManager.h"
#include "PSIFileUtils.h"

#include "PSIRenderScene.h"
#include "PSIRenderObj.h"
#include "PSIRenderMesh.h"

#include "PSIGeometry.h"
#include "PSIGeometryData.h"
#include "PSICubeGeometry.h"
#include "PSICuboidGeometry.h"
//#include "PSISphereGeometry.h"

#include "PSICycler.h"
#include "PSIScaler.h"

#include "PSIFrameTimer.h"

#include "PSILight.h"

#include "PSITextRenderer.h"
#include "PSIGLTFLoader.h"
