// GameplayLintCore - the data walker: every value of one struct type that an asset holds, with a readable path.

#pragma once

#include "CoreMinimal.h"

class FProperty;
class UObject;
class UScriptStruct;
class UStruct;

namespace GameplayLint
{
	/** One value found by the walker. */
	struct FValue
	{
		/** The value's memory: cast it to the struct type that was asked for. */
		const void* Data = nullptr;

		/**
		 * The property a programmer declared, for metadata such as LintRequired: the property itself, or, for an element
		 * of an array, set or map, the container property (that is where UHT puts the metadata).
		 */
		const FProperty* Property = nullptr;

		/** Where the value lives: "Nested.Row", "List[2].Tag", "Row[R05].Link", "Component[Ticker].Payload.Curve". */
		FString Path;
	};

	/**
	 * Calls Visit for every value of type Struct (or a struct derived from it) that Asset holds:
	 *   - a DataTable: every row;
	 *   - a Blueprint: its class default object and every component template, inherited ones included;
	 *   - anything else: the object itself;
	 * and in each of those, nested structs, arrays, sets, maps, optionals, instanced-struct payloads and instanced
	 * sub-objects. Transient properties are skipped: they are not saved data. A matched value is not searched further.
	 */
	GAMEPLAYLINTCORE_API void ForEachValue(UObject* Asset, const UScriptStruct* Struct, TFunctionRef<void(const FValue&)> Visit);

	/** The same, for one block of memory described by Container (a UClass or UScriptStruct). Used by ForEachValue and tests. */
	GAMEPLAYLINTCORE_API void ForEachValueIn(const UStruct* Container, const void* Memory, const UScriptStruct* Struct, const FString& Prefix, TFunctionRef<void(const FValue&)> Visit);

	/** The value of a metadata key on the value's declaring property, or null. Metadata exists in editor builds only. */
	GAMEPLAYLINTCORE_API const FString* FindMetaData(const FValue& Value, FName Key);
}
