// Copyright TSAV. All Rights Reserved.

#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"

#include "TSAVPrevisToolset.generated.h"

/**
 * High-level editor tools for constructing and diagnosing TSAV previs scenes.
 *
 * This module is editor-only. The packaged previs application does not depend
 * on MCP, Codex, or this toolset.
 */
UCLASS(BlueprintType)
class TSAVPREVISTOOLS_API UTSAVPrevisToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns the current DMX transport state as JSON.
	 *
	 * Reports global send/receive settings, configured input and output ports,
	 * registration state, universe ranges, and the age/content summary of each
	 * buffered input universe. A universe is active when its most recent packet
	 * is no older than ActiveWindowSeconds.
	 *
	 * @param ActiveWindowSeconds Maximum packet age used to classify a universe as active.
	 * @return A JSON object describing the current DMX state.
	 */
	UFUNCTION(meta = (AICallable), Category = "TSAV Previs|DMX")
	static FString GetDMXStatus(float ActiveWindowSeconds = 1.0f);
};
