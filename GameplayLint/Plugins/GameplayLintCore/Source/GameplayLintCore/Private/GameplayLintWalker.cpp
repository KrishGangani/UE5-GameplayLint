// GameplayLintCore - the data walker.

#include "GameplayLintWalker.h"

#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace GameplayLint
{
	namespace
	{
		/** The property that carries a value's metadata: an array/set/map element's metadata lives on the container. */
		const FProperty* DeclaringProperty(const FProperty* Property)
		{
			const FProperty* Owner = Property->GetOwner<FProperty>();
			return Owner ? Owner : Property;
		}

		/** Instanced sub-objects found while walking: each object and the path prefix for its own values. */
		using FSubObjects = TArray<TPair<const UObject*, FString>>;

		/**
		 * The one walk over a block of memory. Calls Visit for each matching struct value. If SubObjects is set, also
		 * collects the instanced sub-objects that Owner owns, for the caller to walk next.
		 */
		void Walk(const UStruct* Container, const void* Memory, const UScriptStruct* Struct, const FString& Prefix,
			TFunctionRef<void(const FValue&)> Visit, const UObject* Owner, FSubObjects* SubObjects)
		{
			for (FPropertyValueIterator It(FProperty::StaticClass(), Container, Memory); It; ++It)
			{
				const FProperty* Property = It.Key();
				if (Property->HasAnyPropertyFlags(CPF_Transient))
				{
					It.SkipRecursiveProperty();   // not saved data, e.g. FGameplayTagContainer::ParentTags
					continue;
				}
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
					StructProperty && StructProperty->Struct->IsChildOf(Struct))
				{
					Visit({ It.Value(), DeclaringProperty(Property), Prefix + It.GetPropertyPathDebugString() });
					It.SkipRecursiveProperty();   // a match is a leaf: don't look for the same type inside it
					continue;
				}
				// Instanced sub-objects belong to this asset's data; plain object references point at other assets.
				if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
					SubObjects && ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
				{
					const UObject* SubObject = ObjectProperty->GetObjectPropertyValue(It.Value());
					if (SubObject && SubObject->IsIn(Owner))
					{
						SubObjects->Emplace(SubObject, Prefix + It.GetPropertyPathDebugString() + TEXT("."));
					}
				}
			}
		}

		/** Walks one object's data, then the instanced sub-objects it owns. Visited stops cycles and double visits. */
		void VisitObject(const UObject* Object, const UScriptStruct* Struct, const FString& Prefix, TSet<const UObject*>& Visited,
			TFunctionRef<void(const FValue&)> Visit)
		{
			if (!Object || Visited.Contains(Object))
			{
				return;
			}
			Visited.Add(Object);
			FSubObjects SubObjects;
			Walk(Object->GetClass(), Object, Struct, Prefix, Visit, Object, &SubObjects);
			for (const TPair<const UObject*, FString>& SubObject : SubObjects)
			{
				VisitObject(SubObject.Key, Struct, SubObject.Value, Visited, Visit);
			}
		}
	}

	void ForEachValueIn(const UStruct* Container, const void* Memory, const UScriptStruct* Struct, const FString& Prefix, TFunctionRef<void(const FValue&)> Visit)
	{
		Walk(Container, Memory, Struct, Prefix, Visit, nullptr, nullptr);
	}

	void ForEachValue(UObject* Asset, const UScriptStruct* Struct, TFunctionRef<void(const FValue&)> Visit)
	{
		if (!Asset || !Struct)
		{
			return;
		}

		if (const UDataTable* Table = Cast<UDataTable>(Asset))
		{
			if (const UScriptStruct* RowStruct = Table->GetRowStruct())
			{
				for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
				{
					ForEachValueIn(RowStruct, Row.Value, Struct, FString::Printf(TEXT("Row[%s]."), *Row.Key.ToString()), Visit);
				}
			}
			return;
		}

		TSet<const UObject*> Visited;
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			// The data a Blueprint carries lives in what its compile produced, not in the UBlueprint object.
			UClass* Generated = Blueprint->GeneratedClass;
			if (!Generated)
			{
				return;
			}
			VisitObject(Generated->GetDefaultObject(), Struct, TEXT("Default."), Visited, Visit);
			if (Generated->IsChildOf(AActor::StaticClass()))
			{
				// Native and Blueprint-added templates, inherited ones included, with child overrides (Actor.cpp:7349-7400).
				AActor::ForEachComponentOfActorClassDefault(Generated, UActorComponent::StaticClass(), [&](const UActorComponent* Template)
				{
					VisitObject(Template, Struct, FString::Printf(TEXT("Component[%s]."), *Template->GetName()), Visited, Visit);
					return true;
				});
			}
			return;
		}

		VisitObject(Asset, Struct, FString(), Visited, Visit);
	}

	const FString* FindMetaData(const FValue& Value, FName Key)
	{
#if WITH_METADATA
		return Value.Property ? Value.Property->FindMetaData(Key) : nullptr;
#else
		return nullptr;
#endif
	}
}
