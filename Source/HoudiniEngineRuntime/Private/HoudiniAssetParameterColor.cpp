/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniApi.h"
#include "HoudiniAssetParameterColor.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"

#include "Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetParameterColor::UHoudiniAssetParameterColor( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , Color( FColor::White )
    , bIsColorPickerOpen( false )
{}

UHoudiniAssetParameterColor::~UHoudiniAssetParameterColor()
{}

void
UHoudiniAssetParameterColor::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    if ( Ar.IsLoading() )
        Color = FColor::White;

    Ar << Color;
}

UHoudiniAssetParameterColor *
UHoudiniAssetParameterColor::Create(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    UObject * Outer = InPrimaryObject;
    if ( !Outer )
    {
        Outer = InParentParameter;
        if ( !Outer )
        {
            // Must have either component or parent not null.
            check( false );
        }
    }

    UHoudiniAssetParameterColor * HoudiniAssetParameterColor = NewObject< UHoudiniAssetParameterColor >(
        Outer, UHoudiniAssetParameterColor::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterColor->CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterColor;
}

bool
UHoudiniAssetParameterColor::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId,
    const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle float type.
    if ( ParmInfo.type != HAPI_PARMTYPE_COLOR )
        return false;

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.floatValuesIndex );

    // Get the actual value for this property.
    Color = FLinearColor::White;
    if ( FHoudiniApi::GetParmFloatValues(
        FHoudiniEngine::Get().GetSession(), InNodeId,
        (float *) &Color.R, ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    if ( TupleSize == 3 )
        Color.A = 1.0f;

    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterColor::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    ColorBlock.Reset();

    Super::CreateWidget( LocalDetailCategoryBuilder );

    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( Row, true );

    ColorBlock = SNew( SColorBlock );
    TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );
    VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
    [
        SAssignNew( ColorBlock, SColorBlock )
        .Color( TAttribute< FLinearColor >::Create( TAttribute< FLinearColor >::FGetter::CreateUObject(
            this, &UHoudiniAssetParameterColor::GetColor ) ) )
        .OnMouseButtonDown( FPointerEventHandler::CreateUObject(
            this, &UHoudiniAssetParameterColor::OnColorBlockMouseButtonDown ) )
    ];

    if ( ColorBlock.IsValid() )
        ColorBlock->SetEnabled( !bIsDisabled );

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

#endif // WITH_EDITOR

bool
UHoudiniAssetParameterColor::UploadParameterValue()
{
    if ( FHoudiniApi::SetParmFloatValues(
        FHoudiniEngine::Get().GetSession(), NodeId, (const float*)&Color.R, ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterColor::SetParameterVariantValue( const FVariant& Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    int32 VariantType = Variant.GetType();
    FLinearColor VariantLinearColor = FLinearColor::White;

    if ( EVariantTypes::Color == VariantType )
    {
        FColor VariantColor = Variant.GetValue<FColor>();
        VariantLinearColor = VariantColor.ReinterpretAsLinear();
    }
    else if ( EVariantTypes::LinearColor == VariantType )
    {
        VariantLinearColor = Variant.GetValue< FLinearColor >();
    }
    else
    {
        return false;
    }

#if WITH_EDITOR

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterColorChange", "Houdini Parameter Color: Changing a value" ),
        PrimaryObject );

    Modify();

    if ( !bRecordUndo )
        Transaction.Cancel();

#endif // WITH_EDITOR

    MarkPreChanged( bTriggerModify );
    Color = VariantLinearColor;
    MarkChanged( bTriggerModify );

    return true;
}

FLinearColor
UHoudiniAssetParameterColor::GetColor() const
{
    return Color;
}

#if WITH_EDITOR

bool
UHoudiniAssetParameterColor::IsColorPickerWindowOpen() const
{
    return bIsColorPickerOpen;
}

FReply
UHoudiniAssetParameterColor::OnColorBlockMouseButtonDown( const FGeometry & MyGeometry, const FPointerEvent & MouseEvent )
{
    if ( MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton )
        return FReply::Unhandled();

    FColorPickerArgs PickerArgs;
    PickerArgs.ParentWidget = ColorBlock;
    PickerArgs.bUseAlpha = true;
    PickerArgs.DisplayGamma = TAttribute< float >::Create( TAttribute< float >::FGetter::CreateUObject(
        GEngine, &UEngine::GetDisplayGamma ) );
    PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateUObject(
        this, &UHoudiniAssetParameterColor::OnPaintColorChanged, true, true );
    PickerArgs.InitialColorOverride = GetColor();
    PickerArgs.bOnlyRefreshOnOk = true;
    PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateUObject(
        this, &UHoudiniAssetParameterColor::OnColorPickerClosed );

    OpenColorPicker( PickerArgs );
    bIsColorPickerOpen = true;

    return FReply::Handled();
}

#endif // WITH_EDITOR

void
UHoudiniAssetParameterColor::OnPaintColorChanged( FLinearColor InNewColor, bool bTriggerModify, bool bRecordUndo )
{
    if ( InNewColor != Color )
    {

#if WITH_EDITOR

        // Record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterColorChange", "Houdini Parameter Color: Changing a value" ),
            PrimaryObject );
        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif // WITH_EDITOR

        MarkPreChanged( bTriggerModify );

        Color = InNewColor;

        // Mark this parameter as changed.
        MarkChanged( bTriggerModify );
    }
}

void
UHoudiniAssetParameterColor::OnColorPickerClosed( const TSharedRef< SWindow > & Window )
{
    bIsColorPickerOpen = false;
}

#undef LOCTEXT_NAMESPACE