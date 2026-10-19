// CTConstants.m - CoreText's exported keys and values
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <CoreText/CoreText.h>

// descriptor attributes, the same strings NSFontDescriptor uses
const CFStringRef kCTFontURLAttribute = CFSTR("NSCTFontFileURLAttribute");
const CFStringRef kCTFontNameAttribute = CFSTR("NSFontNameAttribute");
const CFStringRef kCTFontDisplayNameAttribute = CFSTR("NSFontVisibleNameAttribute");
const CFStringRef kCTFontFamilyNameAttribute = CFSTR("NSFontFamilyAttribute");
const CFStringRef kCTFontStyleNameAttribute = CFSTR("NSFontFaceAttribute");
const CFStringRef kCTFontTraitsAttribute = CFSTR("NSCTFontTraitsAttribute");
const CFStringRef kCTFontVariationAttribute = CFSTR("NSFontVariationAttribute");
const CFStringRef kCTFontVariationAxesAttribute = CFSTR("NSCTFontVariationAxesAttribute");
const CFStringRef kCTFontSizeAttribute = CFSTR("NSFontSizeAttribute");
const CFStringRef kCTFontMatrixAttribute = CFSTR("NSFontMatrixAttribute");
const CFStringRef kCTFontCascadeListAttribute = CFSTR("NSCTFontCascadeListAttribute");
const CFStringRef kCTFontCharacterSetAttribute = CFSTR("NSCTFontCharacterSetAttribute");
const CFStringRef kCTFontLanguagesAttribute = CFSTR("NSCTFontLanguagesAttribute");
const CFStringRef kCTFontBaselineAdjustAttribute = CFSTR("NSCTFontBaselineAdjustAttribute");
const CFStringRef kCTFontMacintoshEncodingsAttribute = CFSTR("NSCTFontMacintoshEncodingsAttribute");
const CFStringRef kCTFontFeaturesAttribute = CFSTR("NSCTFontFeaturesAttribute");
const CFStringRef kCTFontFeatureSettingsAttribute = CFSTR("NSCTFontFeatureSettingsAttribute");
const CFStringRef kCTFontFixedAdvanceAttribute = CFSTR("NSCTFontFixedAdvanceAttribute");
const CFStringRef kCTFontOrientationAttribute = CFSTR("NSCTFontOrientationAttribute");
const CFStringRef kCTFontFormatAttribute = CFSTR("NSCTFontFormatAttribute");
const CFStringRef kCTFontRegistrationScopeAttribute = CFSTR("NSCTFontRegistrationScopeAttribute");
const CFStringRef kCTFontPriorityAttribute = CFSTR("NSCTFontPriorityAttribute");
const CFStringRef kCTFontEnabledAttribute = CFSTR("NSCTFontEnabledAttribute");
const CFStringRef kCTFontDownloadableAttribute = CFSTR("NSCTFontDownloadableAttribute");
const CFStringRef kCTFontDownloadedAttribute = CFSTR("NSCTFontDownloadedAttribute");

// traits
const CFStringRef kCTFontSymbolicTrait = CFSTR("NSCTFontSymbolicTrait");
const CFStringRef kCTFontWeightTrait = CFSTR("NSCTFontWeightTrait");
const CFStringRef kCTFontWidthTrait = CFSTR("NSCTFontProportionTrait");
const CFStringRef kCTFontSlantTrait = CFSTR("NSCTFontSlantTrait");

const CGFloat kCTFontWeightUltraLight = -0.8;
const CGFloat kCTFontWeightThin = -0.6;
const CGFloat kCTFontWeightLight = -0.4;
const CGFloat kCTFontWeightRegular = 0.0;
const CGFloat kCTFontWeightMedium = 0.23;
const CGFloat kCTFontWeightSemibold = 0.3;
const CGFloat kCTFontWeightBold = 0.4;
const CGFloat kCTFontWeightHeavy = 0.56;
const CGFloat kCTFontWeightBlack = 0.62;

// name keys
const CFStringRef kCTFontCopyrightNameKey = CFSTR("Copyright");
const CFStringRef kCTFontFamilyNameKey = CFSTR("Family");
const CFStringRef kCTFontSubFamilyNameKey = CFSTR("Subfamily");
const CFStringRef kCTFontStyleNameKey = CFSTR("Style");
const CFStringRef kCTFontUniqueNameKey = CFSTR("Unique");
const CFStringRef kCTFontFullNameKey = CFSTR("Full");
const CFStringRef kCTFontVersionNameKey = CFSTR("Version");
const CFStringRef kCTFontPostScriptNameKey = CFSTR("PostScript");
const CFStringRef kCTFontTrademarkNameKey = CFSTR("Trademark");
const CFStringRef kCTFontManufacturerNameKey = CFSTR("Manufacturer");
const CFStringRef kCTFontDesignerNameKey = CFSTR("Designer");
const CFStringRef kCTFontDescriptionNameKey = CFSTR("Description");
const CFStringRef kCTFontVendorURLNameKey = CFSTR("VendorURL");
const CFStringRef kCTFontDesignerURLNameKey = CFSTR("DesignerURL");
const CFStringRef kCTFontLicenseNameKey = CFSTR("License");
const CFStringRef kCTFontLicenseURLNameKey = CFSTR("LicenseURL");
const CFStringRef kCTFontSampleTextNameKey = CFSTR("SampleText");
const CFStringRef kCTFontPostScriptCIDNameKey = CFSTR("PostScriptCID");

// variation axes and features
const CFStringRef kCTFontVariationAxisIdentifierKey = CFSTR("NSCTVariationAxisIdentifier");
const CFStringRef kCTFontVariationAxisMinimumValueKey = CFSTR("NSCTVariationAxisMinimumValue");
const CFStringRef kCTFontVariationAxisMaximumValueKey = CFSTR("NSCTVariationAxisMaximumValue");
const CFStringRef kCTFontVariationAxisDefaultValueKey = CFSTR("NSCTVariationAxisDefaultValue");
const CFStringRef kCTFontVariationAxisNameKey = CFSTR("NSCTVariationAxisName");
const CFStringRef kCTFontVariationAxisHiddenKey = CFSTR("NSCTVariationAxisHidden");
const CFStringRef kCTFontOpenTypeFeatureTag = CFSTR("CTFeatureOpenTypeTag");
const CFStringRef kCTFontOpenTypeFeatureValue = CFSTR("CTFeatureOpenTypeValue");
const CFStringRef kCTFontFeatureTypeIdentifierKey = CFSTR("CTFeatureTypeIdentifier");
const CFStringRef kCTFontFeatureTypeNameKey = CFSTR("CTFeatureTypeName");
const CFStringRef kCTFontFeatureTypeExclusiveKey = CFSTR("CTFeatureTypeExclusive");
const CFStringRef kCTFontFeatureTypeSelectorsKey = CFSTR("CTFeatureTypeSelectors");
const CFStringRef kCTFontFeatureSelectorIdentifierKey = CFSTR("CTFeatureSelectorIdentifier");
const CFStringRef kCTFontFeatureSelectorNameKey = CFSTR("CTFeatureSelectorName");
const CFStringRef kCTFontFeatureSelectorDefaultKey = CFSTR("CTFeatureSelectorDefault");
const CFStringRef kCTFontFeatureSelectorSettingKey = CFSTR("CTFeatureSelectorSetting");
const CFStringRef kCTFontFeatureSampleTextKey = CFSTR("CTFeatureSampleText");
const CFStringRef kCTFontFeatureTooltipTextKey = CFSTR("CTFeatureTooltipText");

// string attributes, the NS ones toll-free with AppKit's attribute names
const CFStringRef kCTFontAttributeName = CFSTR("NSFont");
const CFStringRef kCTForegroundColorFromContextAttributeName = CFSTR("CTForegroundColorFromContext");
const CFStringRef kCTKernAttributeName = CFSTR("NSKern");
const CFStringRef kCTTrackingAttributeName = CFSTR("CTTracking");
const CFStringRef kCTLigatureAttributeName = CFSTR("NSLigature");
const CFStringRef kCTForegroundColorAttributeName = CFSTR("CTForegroundColor");
const CFStringRef kCTBackgroundColorAttributeName = CFSTR("CTBackgroundColor");
const CFStringRef kCTParagraphStyleAttributeName = CFSTR("NSParagraphStyle");
const CFStringRef kCTStrokeWidthAttributeName = CFSTR("NSStrokeWidth");
const CFStringRef kCTStrokeColorAttributeName = CFSTR("CTStrokeColor");
const CFStringRef kCTUnderlineStyleAttributeName = CFSTR("NSUnderline");
const CFStringRef kCTSuperscriptAttributeName = CFSTR("NSSuperScript");
const CFStringRef kCTUnderlineColorAttributeName = CFSTR("CTUnderlineColor");
const CFStringRef kCTVerticalFormsAttributeName = CFSTR("CTVerticalForms");
const CFStringRef kCTHorizontalInVerticalFormsAttributeName = CFSTR("CTHorizontalInVerticalForms");
const CFStringRef kCTGlyphInfoAttributeName = CFSTR("NSGlyphInfo");
const CFStringRef kCTCharacterShapeAttributeName = CFSTR("NSCharacterShape");
const CFStringRef kCTLanguageAttributeName = CFSTR("NSLanguage");
const CFStringRef kCTRunDelegateAttributeName = CFSTR("CTRunDelegate");
const CFStringRef kCTBaselineClassAttributeName = CFSTR("CTBaselineClass");
const CFStringRef kCTBaselineInfoAttributeName = CFSTR("CTBaselineInfo");
const CFStringRef kCTBaselineReferenceInfoAttributeName = CFSTR("CTBaselineReferenceInfo");
const CFStringRef kCTBaselineOffsetAttributeName = CFSTR("CTBaselineOffset");
const CFStringRef kCTWritingDirectionAttributeName = CFSTR("NSWritingDirection");
const CFStringRef kCTRubyAnnotationAttributeName = CFSTR("CTRubyAnnotation");

// paragraph and frame
const CFStringRef kCTTabColumnTerminatorsAttributeName = CFSTR("NSTabColumnTerminatorsAttributeName");
const CFStringRef kCTFrameProgressionAttributeName = CFSTR("CTFrameProgression");
const CFStringRef kCTFramePathFillRuleAttributeName = CFSTR("CTFramePathFillRule");
const CFStringRef kCTFramePathWidthAttributeName = CFSTR("CTFramePathWidth");
const CFStringRef kCTFrameClippingPathsAttributeName = CFSTR("CTFrameClippingPaths");
const CFStringRef kCTFramePathClippingPathAttributeName = CFSTR("CTFramePathClippingPath");

// typesetter options
const CFStringRef kCTTypesetterOptionAllowUnboundedLayout = CFSTR("CTTypesetterOptionAllowUnboundedLayout");
const CFStringRef kCTTypesetterOptionDisableBidiProcessing = CFSTR("CTTypesetterOptionDisableBidiProcessing");
const CFStringRef kCTTypesetterOptionForcedEmbeddingLevel = CFSTR("CTTypesetterOptionForcedEmbeddingLevel");

// font manager and collections
const CFStringRef kCTFontManagerErrorDomain = CFSTR("com.apple.CoreText.CTFontManagerErrorDomain");
const CFStringRef kCTFontManagerErrorFontURLsKey = CFSTR("CTFontManagerErrorFontURLs");
const CFStringRef kCTFontManagerRegisteredFontsChangedNotification = CFSTR("CTFontManagerFontChangedNotification");
const CFStringRef kCTFontCollectionRemoveDuplicatesOption = CFSTR("NSFontCollectionRemoveDuplicates");
const CFStringRef kCTFontCollectionIncludeDisabledFontsOption = CFSTR("NSFontCollectionIncludeDisabledFonts");
const CFStringRef kCTFontCollectionDisallowAutoActivationOption = CFSTR("NSFontCollectionDisallowAutoActivation");

// the CoreText version macOS 15 reports, so version checks take the modern paths
uint32_t CTGetCoreTextVersion(void) {
    return 0x00110000;
}
