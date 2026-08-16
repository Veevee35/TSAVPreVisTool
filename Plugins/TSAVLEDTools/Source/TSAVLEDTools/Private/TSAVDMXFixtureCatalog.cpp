// Copyright TSAV. All Rights Reserved.

#include "TSAVDMXFixtureCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TSAVDMXFixtureCatalog)

const FSoftObjectPath UTSAVDMXFixtureCatalog::DefaultCatalogPath(
	TEXT("/Game/TSAV/Fixtures/DMX/DA_TSAVFixtureCatalog.DA_TSAVFixtureCatalog"));

const FTSAVDMXFixtureDefinition* UTSAVDMXFixtureCatalog::FindFixture(const FName DefinitionId) const
{
	return Fixtures.FindByPredicate([DefinitionId](const FTSAVDMXFixtureDefinition& Fixture)
	{
		return Fixture.DefinitionId == DefinitionId;
	});
}
