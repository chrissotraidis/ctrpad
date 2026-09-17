#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import "native_diagnostics.h"
#include "platform/native_log.h"
#include <sys/utsname.h>

#include <math.h>

#include "platform/native_input.h"
#include "platform/native_ios_touch.h"
#include "platform/native_renderer.h"

typedef NS_ENUM(NSInteger, CTRPadTouchHandedness)
{
	CTRPadTouchHandednessSteerLeft = 0,
	CTRPadTouchHandednessSteerRight,
};

typedef NS_ENUM(NSInteger, CTRPadTouchSize)
{
	CTRPadTouchSizeSmall = 0,
	CTRPadTouchSizeStandard,
	CTRPadTouchSizeLarge,
};

static NSString *const s_touchHandednessKey = @"CTRPadTouchHandedness";
static NSString *const s_touchSizeKey = @"CTRPadTouchSize";
static NSString *const s_touchOpacityKey = @"CTRPadTouchOpacity";
static NSString *const s_touchEnabledKey = @"CTRPadTouchEnabled";
static NSString *const s_simplifiedRacingControlsKey = @"CTRPadSimplifiedRacingControls";
static NSString *const s_touchLayoutKeyPrefix = @"CTRPadTouchLayout";
static NSString *const s_internalResolutionScaleKey = @"CTRPadInternalResolutionScale";
static const NSTimeInterval s_gasLatchDelay = 2.0;
static const CGFloat s_touchOpacityMinimum = 0.10;
static const CGFloat s_touchOpacityDefault = 0.58;
static const CGFloat s_touchOpacityMaximum = 1.0;

@class CTRPadTouchOverlayViewController;
@class CTRPadTouchStickView;

@interface CTRPadTouchPassthroughView : UIView
@end

@interface CTRPadInputButton : UIButton
@property(nonatomic, assign) BOOL accessibilityHeld;
@property(nonatomic, assign) BOOL accessibilityTapPending;
@property(nonatomic, assign) NSUInteger accessibilityActionGeneration;
@property(nonatomic, assign) BOOL inputPressed;
@property(nonatomic, assign) BOOL inputLatched;
@property(nonatomic, assign) BOOL holdToLatch;
@property(nonatomic, assign) BOOL latchOnTap;
@property(nonatomic, assign) BOOL layoutEditing;
@property(nonatomic, assign) NSUInteger latchGeneration;
@property(nonatomic, strong) UIColor *baseColor;
@property(nonatomic, assign) CGFloat controlOpacity;
@property(nonatomic, copy) NSString *releasedTitle;
@property(nonatomic, copy) NSString *latchedTitle;
@property(nonatomic, copy) NSString *latchEngagedAnnouncement;
@property(nonatomic, copy) NSString *latchReleasedAnnouncement;
@property(nonatomic, strong) CAShapeLayer *holdProgressLayer;
- (void)updateAppearance;
- (void)beginHoldProgress;
- (void)stopHoldProgress;
- (void)cancelInput;
- (BOOL)accessibilityPressBriefly;
- (BOOL)accessibilityPressThreeSeconds;
- (BOOL)accessibilityHoldControl;
- (BOOL)accessibilityReleaseControl;
@end

@interface CTRPadTouchStickView : UIView
@property(nonatomic, strong) UIView *knob;
@property(nonatomic, strong) UILabel *label;
@property(nonatomic, assign) BOOL trackingTouch;
@property(nonatomic, assign) BOOL accessibilitySteering;
@property(nonatomic, assign) NSUInteger accessibilitySteeringGeneration;
@property(nonatomic, assign) unsigned int directionMask;
@property(nonatomic, assign) BOOL layoutEditing;
@property(nonatomic, assign) BOOL dynamicOriginActive;
@property(nonatomic, assign) BOOL horizontalOnly;
@property(nonatomic, assign) CGPoint restingCenter;
@property(nonatomic, assign) CGFloat controlOpacity;
@property(nonatomic, copy) void (^releaseHandler)(void);
- (void)applyControlOpacity:(CGFloat)opacity;
- (void)beginDynamicTouchAtPoint:(CGPoint)point inView:(UIView *)view;
- (void)continueDynamicTouchAtPoint:(CGPoint)point inView:(UIView *)view;
- (void)publishPoint:(CGPoint)point;
- (void)releaseTouch;
- (BOOL)accessibilityHoldLeft;
- (BOOL)accessibilityHoldRight;
- (BOOL)accessibilityNudgeLeft;
- (BOOL)accessibilityNudgeRight;
- (BOOL)accessibilityHoldSlightLeft;
- (BOOL)accessibilityHoldSlightRight;
- (BOOL)accessibilityNudgeSlightLeft;
- (BOOL)accessibilityNudgeSlightRight;
- (BOOL)accessibilityHoldUp;
- (BOOL)accessibilityHoldDown;
- (BOOL)accessibilityCenter;
@end

@interface CTRPadTouchSteeringZoneView : UIView
@property(nonatomic, weak) CTRPadTouchStickView *stick;
@property(nonatomic, assign) BOOL layoutEditing;
@end

@interface CTRPadTouchOverlayViewController : UIViewController
@property(nonatomic, assign) CTRPadTouchHandedness handedness;
@property(nonatomic, assign) CGFloat controlScale;
@property(nonatomic, assign) CGFloat controlOpacity;
@property(nonatomic, assign) BOOL touchControlsEnabled;
@property(nonatomic, assign) BOOL simplifiedRacingControls;
@property(nonatomic, assign) BOOL layoutEditing;
@property(nonatomic, strong) CTRPadTouchSteeringZoneView *steeringZone;
@property(nonatomic, weak) CTRPadInputButton *driftButton;
@property(nonatomic, strong) NSArray<UIView *> *editableControls;
@property(nonatomic, strong) NSMutableArray<UIGestureRecognizer *> *editGestures;
@property(nonatomic, strong) NSMutableDictionary<NSString *, NSArray<NSNumber *> *> *layoutCenters;
@property(nonatomic, strong) NSMutableDictionary<NSString *, NSNumber *> *layoutScales;
@property(nonatomic, copy) NSString *layoutProfile;
@property(nonatomic, weak) UIView *selectedControl;
@property(nonatomic, strong) UIView *editorPanel;
@property(nonatomic, strong) UILabel *editorLabel;
@property(nonatomic, strong) UIButton *editorSmallerButton;
@property(nonatomic, strong) UIButton *editorLargerButton;
@property(nonatomic, strong) UIButton *editorResetButton;
@property(nonatomic, strong) UIButton *editorDoneButton;
- (void)applyPreferencesAndRebuildControls;
- (void)resetControlState;
- (void)releaseLatchedDrift;
- (void)beginLayoutEditing;
- (void)endLayoutEditing;
- (void)resetCurrentLayout;
- (void)resetAllLayouts;
- (void)installLayoutEditor;
- (void)layoutControls;
- (void)selectControl:(UIGestureRecognizer *)gesture;
- (void)shrinkSelectedControl;
- (void)growSelectedControl;
- (void)presentDiscReselectionConfirmation;
@end

@interface CTRPadTouchSettingsViewController : UIViewController
@property(nonatomic, weak) CTRPadTouchOverlayViewController *overlayController;
@property(nonatomic, strong) UISegmentedControl *handednessControl;
@property(nonatomic, strong) UISegmentedControl *sizeControl;
@property(nonatomic, strong) UISlider *opacitySlider;
@property(nonatomic, strong) UILabel *opacityValueLabel;
@property(nonatomic, strong) UISegmentedControl *internalResolutionControl;
@property(nonatomic, strong) UISwitch *touchEnabledSwitch;
@property(nonatomic, strong) UISwitch *classicControlsSwitch;
@property(nonatomic, strong) UIButton *editLayoutButton;
@end

static CTRPadTouchOverlayViewController *s_touchOverlayController;
static NativeIOSTouchDiscReselectionCallback s_discReselectionCallback;
static void *s_discReselectionUserdata;

static NSInteger CTRPadTouch_ReadPreference(NSString *key, NSInteger defaultValue, NSInteger maximumValue)
{
	id value = [NSUserDefaults.standardUserDefaults objectForKey:key];
	if (![value isKindOfClass:NSNumber.class])
	{
		return defaultValue;
	}

	NSInteger choice = [value integerValue];
	return ((choice >= 0) && (choice <= maximumValue)) ? choice : defaultValue;
}

static CGFloat CTRPadTouch_ScaleForChoice(CTRPadTouchSize choice)
{
	switch (choice)
	{
	case CTRPadTouchSizeSmall:
		return 0.88;
	case CTRPadTouchSizeLarge:
		return 1.12;
	case CTRPadTouchSizeStandard:
		return 1.0;
	}
	return 1.0;
}

static NSInteger CTRPadTouch_DefaultSize(void)
{
    return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad ? CTRPadTouchSizeLarge : CTRPadTouchSizeStandard;
}

static CGFloat CTRPadTouch_DefaultOpacity(void)
{
    return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad ? 0.5347171426 : s_touchOpacityDefault;
}

// Owner-selected iPad layout, normalized to the usable safe area. Existing
// saved profiles take precedence; phone and classic defaults are unchanged.
static NSDictionary *CTRPadTouch_DefaultLayout(NSString *profile)
{
    if (![profile isEqualToString:@"tablet-left-simplified-v1"]) return nil;
    return @{ @"centers": @{
        @"ctrpad.touch.circle": @[ @(0.9494403535), @(0.7972489960) ],
        @"ctrpad.touch.cross": @[ @(0.9271723122), @(0.8898995984) ],
        @"ctrpad.touch.l1": @[ @(0.8053902798), @(0.7767269076) ],
        @"ctrpad.touch.r1": @[ @(0.9265095729), @(0.7179919679) ],
        @"ctrpad.touch.select": @[ @(0.9451693667), @(0.6575502008) ],
        @"ctrpad.touch.square": @[ @(0.8630927835), @(0.8931526104) ],
        @"ctrpad.touch.start": @[ @(0.9451693667), @(0.6071285141) ],
        @"ctrpad.touch.stick": @[ @(0.1312518409), @(0.8658433735) ],
        @"ctrpad.touch.triangle": @[ @(0.8895287187), @(0.8183333333) ],
    }, @"scales": @{} };
}

static CGFloat CTRPadTouch_ReadOpacity(void)
{
	id value = [NSUserDefaults.standardUserDefaults objectForKey:s_touchOpacityKey];
	if (![value isKindOfClass:NSNumber.class])
	{
		return CTRPadTouch_DefaultOpacity();
	}

	NSNumber *number = (NSNumber *)value;
	if (CFNumberIsFloatType((__bridge CFNumberRef)number))
	{
		return MIN(s_touchOpacityMaximum, MAX(s_touchOpacityMinimum, number.doubleValue));
	}

	// Migrate the original Low / Standard / High integer preference in place.
	switch (number.integerValue)
	{
	case 0:
		return 0.36;
	case 2:
		return 0.80;
	case 1:
		return 0.58;
	}
	return CTRPadTouch_DefaultOpacity();
}

static CGFloat CTRPadTouch_RelativeOpacity(CGFloat opacity)
{
	return opacity / s_touchOpacityDefault;
}

@implementation CTRPadTouchPassthroughView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
	UIView *result = [super hitTest:point withEvent:event];
	return result == self ? nil : result;
}

@end

@implementation CTRPadInputButton

- (void)updateAppearance
{
	CGFloat relativeOpacity = CTRPadTouch_RelativeOpacity(self.controlOpacity);
	UIColor *color = self.inputLatched
	                     ? [UIColor colorWithRed:0.10 green:0.52 blue:0.98 alpha:1.0]
	                     : (self.baseColor ?: [UIColor colorWithWhite:0.08 alpha:1.0]);
	CGFloat red = 0, green = 0, blue = 0;
    [color getRed:&red green:&green blue:&blue alpha:NULL];
    UIColor *surface = self.inputLatched ? color : [UIColor colorWithRed:0.035 + red * 0.28
        green:0.045 + green * 0.28 blue:0.075 + blue * 0.28 alpha:1];
    self.backgroundColor = [surface colorWithAlphaComponent:self.inputLatched ? MAX(0.82, self.controlOpacity) : self.controlOpacity];
	self.layer.borderColor = (self.inputLatched
	                              ? [UIColor colorWithRed:0.72 green:0.88 blue:1.0 alpha:1.0]
	                              : [color colorWithAlphaComponent:MIN(1.0, 0.95 * relativeOpacity)])
	                             .CGColor;
	[self setTitleColor:[UIColor colorWithWhite:1.0 alpha:self.inputLatched ? 1.0 : MIN(1.0, relativeOpacity)]
	             forState:UIControlStateNormal];
	NSString *title = self.inputLatched ? self.latchedTitle : self.releasedTitle;
	if (title.length > 0)
	{
		[self setTitle:title forState:UIControlStateNormal];
	}
	self.alpha = 1.0;
	self.layer.borderWidth = (self.inputPressed || self.inputLatched) ? 2.5 : 1.0;
}

- (void)layoutSubviews
{
	[super layoutSubviews];
	if (self.holdProgressLayer != nil)
	{
		CGRect ringBounds = CGRectInset(self.bounds, 4.0, 4.0);
		self.holdProgressLayer.frame = self.bounds;
		self.holdProgressLayer.path = [UIBezierPath bezierPathWithRoundedRect:ringBounds
		                                                           cornerRadius:MIN(CGRectGetWidth(ringBounds), CGRectGetHeight(ringBounds)) * 0.5]
		                                      .CGPath;
	}
}

- (void)beginHoldProgress
{
	if (self.holdProgressLayer == nil)
	{
		return;
	}
	[self.holdProgressLayer removeAllAnimations];
	self.holdProgressLayer.hidden = NO;
	[CATransaction begin];
	[CATransaction setDisableActions:YES];
	self.holdProgressLayer.strokeEnd = 1.0;
	[CATransaction commit];
	CABasicAnimation *animation = [CABasicAnimation animationWithKeyPath:@"strokeEnd"];
	animation.fromValue = @0.0;
	animation.toValue = @1.0;
	animation.duration = s_gasLatchDelay;
	animation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
	[self.holdProgressLayer addAnimation:animation forKey:@"ctrpad.gas-lock-progress"];
}

- (void)stopHoldProgress
{
	if (self.holdProgressLayer == nil)
	{
		return;
	}
	[self.holdProgressLayer removeAllAnimations];
	self.holdProgressLayer.hidden = YES;
}

- (void)cancelInput
{
	self.accessibilityActionGeneration += 1;
	self.latchGeneration += 1;
	self.accessibilityHeld = NO;
	self.accessibilityTapPending = NO;
	self.inputPressed = NO;
	self.inputLatched = NO;
	self.accessibilityValue = @"Released";
	[self stopHoldProgress];
	[self updateAppearance];
}

- (BOOL)accessibilityPressForDuration:(NSTimeInterval)duration value:(NSString *)value
{
	if (self.inputLatched)
	{
		return [self accessibilityReleaseControl];
	}
	[self accessibilityReleaseControl];
	NSUInteger generation = ++self.accessibilityActionGeneration;
	self.accessibilityTapPending = YES;
	self.accessibilityValue = value;
	[self sendActionsForControlEvents:UIControlEventTouchDown];
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(duration * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		if (self.accessibilityTapPending && !self.accessibilityHeld &&
		    (self.accessibilityActionGeneration == generation))
		{
			self.accessibilityTapPending = NO;
			self.accessibilityValue = self.inputLatched ? @"Gas locked" : @"Released";
			[self sendActionsForControlEvents:UIControlEventTouchUpInside];
			if (self.inputLatched)
			{
				self.accessibilityValue = @"Gas locked";
			}
		}
	});
	return YES;
}

- (BOOL)accessibilityActivate
{
	// Voice Control, Switch Control and Simulator accessibility actions invoke
	// a button's primary action without synthesizing UIControlEventTouchDown.
	// Publish the same down/up pair as a physical finger so accessible controls
	// cannot silently appear to press while the retail pad receives no edge.
	return [self accessibilityPressForDuration:0.10 value:@"Pressed"];
}

- (BOOL)accessibilityPressBriefly
{
	return [self accessibilityPressForDuration:1.0 value:@"Pressed for one second"];
}

- (BOOL)accessibilityPressThreeSeconds
{
	return [self accessibilityPressForDuration:3.0 value:@"Pressed for three seconds"];
}

- (BOOL)accessibilityHoldControl
{
	BOOL wasActive = self.accessibilityHeld || self.accessibilityTapPending;
	self.accessibilityActionGeneration += 1;
	self.accessibilityTapPending = NO;
	self.accessibilityHeld = YES;
	self.accessibilityValue = @"Held";
	if (!wasActive)
	{
		[self sendActionsForControlEvents:UIControlEventTouchDown];
	}
	return YES;
}

- (BOOL)accessibilityReleaseControl
{
	if (self.inputLatched)
	{
		self.accessibilityActionGeneration += 1;
		self.accessibilityHeld = NO;
		self.accessibilityTapPending = NO;
		[self sendActionsForControlEvents:UIControlEventTouchDown];
		return YES;
	}
	BOOL wasActive = self.accessibilityHeld || self.accessibilityTapPending;
	self.accessibilityActionGeneration += 1;
	self.accessibilityHeld = NO;
	self.accessibilityTapPending = NO;
	self.accessibilityValue = @"Released";
	if (wasActive)
	{
		[self sendActionsForControlEvents:UIControlEventTouchUpInside];
	}
	return YES;
}

@end

@implementation CTRPadTouchSteeringZoneView

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	if (self.layoutEditing)
	{
		return;
	}
	UITouch *touch = touches.anyObject;
	if ((touch != nil) && (self.stick != nil))
	{
		[self.stick beginDynamicTouchAtPoint:[touch locationInView:self] inView:self];
	}
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	UITouch *touch = touches.anyObject;
	if (!self.layoutEditing && (touch != nil) && self.stick.trackingTouch)
	{
		[self.stick continueDynamicTouchAtPoint:[touch locationInView:self] inView:self];
	}
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)touches;
	(void)event;
	[self.stick releaseTouch];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)touches;
	(void)event;
	[self.stick releaseTouch];
}

@end

@implementation CTRPadTouchStickView

- (void)updateVisualAppearance
{
	BOOL visible = self.layoutEditing || self.trackingTouch;
	CGFloat relativeOpacity = CTRPadTouch_RelativeOpacity(self.controlOpacity);
	self.backgroundColor = visible ? [UIColor colorWithWhite:0.04 alpha:self.controlOpacity * 0.72] : UIColor.clearColor;
	self.layer.borderColor = (visible ? [UIColor colorWithWhite:1.0 alpha:MIN(1.0, self.controlOpacity * 0.82)]
	                                  : UIColor.clearColor)
	                             .CGColor;
	self.knob.hidden = !visible;
	self.label.hidden = !visible || self.horizontalOnly;
	if (visible)
	{
		self.knob.backgroundColor = [UIColor colorWithRed:0.16
		                                           green:0.48
		                                            blue:0.95
		                                           alpha:MIN(1.0, 0.76 * relativeOpacity)];
		self.knob.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:MIN(1.0, 0.72 * relativeOpacity)].CGColor;
		self.label.textColor = [UIColor colorWithWhite:1.0 alpha:MIN(1.0, 0.74 * relativeOpacity)];
	}
}

- (instancetype)init
{
	self = [super initWithFrame:CGRectZero];
	if (self != nil)
	{
		self.translatesAutoresizingMaskIntoConstraints = NO;
		self.multipleTouchEnabled = NO;
		self.accessibilityIdentifier = @"ctrpad.touch.stick";
		self.accessibilityLabel = @"Steering stick";
		self.accessibilityValue = @"Centered";
		self.isAccessibilityElement = YES;
		self.accessibilityCustomActions = @[
			[[UIAccessibilityCustomAction alloc] initWithName:@"Nudge left" target:self selector:@selector(accessibilityNudgeLeft)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Nudge right" target:self selector:@selector(accessibilityNudgeRight)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Hold left" target:self selector:@selector(accessibilityHoldLeft)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Hold right" target:self selector:@selector(accessibilityHoldRight)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Nudge slight left" target:self selector:@selector(accessibilityNudgeSlightLeft)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Nudge slight right" target:self selector:@selector(accessibilityNudgeSlightRight)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Hold slight left" target:self selector:@selector(accessibilityHoldSlightLeft)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Hold slight right" target:self selector:@selector(accessibilityHoldSlightRight)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Hold up" target:self selector:@selector(accessibilityHoldUp)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Hold down" target:self selector:@selector(accessibilityHoldDown)],
			[[UIAccessibilityCustomAction alloc] initWithName:@"Center" target:self selector:@selector(accessibilityCenter)],
		];
		self.backgroundColor = [UIColor colorWithWhite:0.04 alpha:0.42];
		self.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.42].CGColor;
		self.layer.borderWidth = 2.0;

		self.knob = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 70, 70)];
		self.knob.userInteractionEnabled = NO;
		self.knob.backgroundColor = [UIColor colorWithRed:0.16 green:0.48 blue:0.95 alpha:0.76];
		self.knob.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.72].CGColor;
		self.knob.layer.borderWidth = 2.0;
		[self addSubview:self.knob];

		self.label = [[UILabel alloc] initWithFrame:CGRectZero];
		self.label.translatesAutoresizingMaskIntoConstraints = NO;
		self.label.text = @"STEER";
		self.label.textColor = [UIColor colorWithWhite:1.0 alpha:0.74];
		self.label.font = [UIFont systemFontOfSize:12.0 weight:UIFontWeightBold];
		self.label.userInteractionEnabled = NO;
		[self addSubview:self.label];
		[NSLayoutConstraint activateConstraints:@[
			[self.label.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
			[self.label.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-12.0],
		]];
	}
	return self;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
	// The transparent steering zone owns gameplay touches. The ring itself only
	// becomes directly draggable while editing the saved layout.
	return self.layoutEditing && [super pointInside:point withEvent:event];
}

- (void)setLayoutEditing:(BOOL)layoutEditing
{
	_layoutEditing = layoutEditing;
	[self updateVisualAppearance];
}

- (BOOL)accessibilityHoldX:(CGFloat)x y:(CGFloat)y value:(NSString *)value
{
	[self releaseTouch];
	self.accessibilitySteering = YES;
	self.trackingTouch = YES;
	[self updateVisualAppearance];
	CGPoint center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
	CGFloat radius = MAX(1.0, MIN(self.bounds.size.width, self.bounds.size.height) * 0.5 - self.knob.bounds.size.width * 0.5 - 4.0);
	[self publishPoint:CGPointMake(center.x + x * radius, center.y + y * radius)];
	self.accessibilityValue = value;
	return YES;
}

- (BOOL)accessibilityNudgeX:(CGFloat)x y:(CGFloat)y value:(NSString *)value
{
	[self accessibilityHoldX:x y:y value:value];
	NSUInteger generation = self.accessibilitySteeringGeneration;
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.45 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
		// A newer accessibility action or a physical touch owns the stick now.
		// Do not let an older nudge unexpectedly center that newer input.
		if (self.accessibilitySteering && (self.accessibilitySteeringGeneration == generation))
		{
			[self releaseTouch];
		}
	});
	return YES;
}

- (BOOL)accessibilityHoldLeft
{
	return [self accessibilityHoldX:-1.0 y:0.0 value:@"Held left"];
}

- (BOOL)accessibilityHoldRight
{
	return [self accessibilityHoldX:1.0 y:0.0 value:@"Held right"];
}

- (BOOL)accessibilityNudgeLeft
{
	return [self accessibilityNudgeX:-1.0 y:0.0 value:@"Nudging left"];
}

- (BOOL)accessibilityNudgeRight
{
	return [self accessibilityNudgeX:1.0 y:0.0 value:@"Nudging right"];
}

- (BOOL)accessibilityHoldSlightLeft
{
	return [self accessibilityHoldX:-0.45 y:0.0 value:@"Held slight left"];
}

- (BOOL)accessibilityHoldSlightRight
{
	return [self accessibilityHoldX:0.45 y:0.0 value:@"Held slight right"];
}

- (BOOL)accessibilityNudgeSlightLeft
{
	return [self accessibilityNudgeX:-0.45 y:0.0 value:@"Nudging slight left"];
}

- (BOOL)accessibilityNudgeSlightRight
{
	return [self accessibilityNudgeX:0.45 y:0.0 value:@"Nudging slight right"];
}

- (BOOL)accessibilityHoldUp
{
	return [self accessibilityHoldX:0.0 y:-1.0 value:@"Held up"];
}

- (BOOL)accessibilityHoldDown
{
	return [self accessibilityHoldX:0.0 y:1.0 value:@"Held down"];
}

- (BOOL)accessibilityCenter
{
	[self releaseTouch];
	return YES;
}

- (void)applyControlOpacity:(CGFloat)opacity
{
	self.controlOpacity = opacity;
	[self updateVisualAppearance];
}

- (void)layoutSubviews
{
	[super layoutSubviews];
	self.layer.cornerRadius = self.horizontalOnly ? self.bounds.size.height * 0.5
	                                           : MIN(self.bounds.size.width, self.bounds.size.height) * 0.5;
	self.knob.layer.cornerRadius = self.knob.bounds.size.width * 0.5;
	if (!self.trackingTouch)
	{
		self.knob.center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
	}
}

- (void)publishPoint:(CGPoint)point
{
	CGPoint center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
	CGFloat dx = point.x - center.x;
	CGFloat dy = point.y - center.y;
	CGFloat horizontalRadius = MAX(1.0, self.bounds.size.width * 0.5 - self.knob.bounds.size.width * 0.5 - 4.0);
	CGFloat radialRadius = MAX(1.0, MIN(self.bounds.size.width, self.bounds.size.height) * 0.5 - self.knob.bounds.size.width * 0.5 - 4.0);
	CGFloat radius = self.horizontalOnly ? horizontalRadius : radialRadius;
	CGFloat verticalDirection = dy;
	unsigned int directionMask = 0;
	if (self.horizontalOnly)
	{
		dx = MIN(radius, MAX(-radius, dx));
		dy = 0.0;
	}
	else
	{
		CGFloat length = hypot(dx, dy);
		if (length > radius)
		{
			dx = dx * radius / length;
			dy = dy * radius / length;
		}
		verticalDirection = dy;
	}
	self.knob.center = CGPointMake(center.x + dx, center.y + dy);
	Platform_InputTouchLeftStick((int)lrint(dx * 32767.0 / radius), (int)lrint(dy * 32767.0 / radius), 1);

	// Preserve the full analog range for racing. The outer ring additionally
	// publishes retail D-pad edges so brief menu gestures are never dependent
	// on the game's optional analog-to-button setting.
	CGFloat horizontalDirectionThreshold = radius * 0.68;
	// Horizontal steering leaves Y available for menu navigation. Use a shorter
	// vertical throw so Up/Down remains comfortable without affecting analog Y.
	CGFloat verticalDirectionThreshold = self.horizontalOnly
	                                         ? MAX(24.0, self.bounds.size.height * 0.28)
	                                         : horizontalDirectionThreshold;
	if (dx <= -horizontalDirectionThreshold)
		directionMask |= PLATFORM_INPUT_TOUCH_LEFT;
	if (dx >= horizontalDirectionThreshold)
		directionMask |= PLATFORM_INPUT_TOUCH_RIGHT;
	if (verticalDirection <= -verticalDirectionThreshold)
		directionMask |= PLATFORM_INPUT_TOUCH_UP;
	if (verticalDirection >= verticalDirectionThreshold)
		directionMask |= PLATFORM_INPUT_TOUCH_DOWN;

	unsigned int releasedDirections = self.directionMask & ~directionMask;
	unsigned int pressedDirections = directionMask & ~self.directionMask;
	if (releasedDirections != 0)
		Platform_InputTouchButton(releasedDirections, 0);
	if (pressedDirections != 0)
		Platform_InputTouchButton(pressedDirections, 1);
	self.directionMask = directionMask;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	if (self.layoutEditing)
	{
		return;
	}
	UITouch *touch = touches.anyObject;
	if (touch == nil)
	{
		return;
	}
	if (self.accessibilitySteering)
	{
		[self releaseTouch];
	}
	self.trackingTouch = YES;
	[self updateVisualAppearance];
	[self publishPoint:[touch locationInView:self]];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	if (self.layoutEditing)
	{
		return;
	}
	UITouch *touch = touches.anyObject;
	if ((touch != nil) && self.trackingTouch)
	{
		[self publishPoint:[touch locationInView:self]];
	}
}

- (void)beginDynamicTouchAtPoint:(CGPoint)point inView:(UIView *)view
{
	if (self.layoutEditing || (view == nil) || (self.superview == nil))
	{
		return;
	}
	[self releaseTouch];
	self.dynamicOriginActive = YES;
	self.center = [view convertPoint:point toView:self.superview];
	self.trackingTouch = YES;
	[self updateVisualAppearance];
	[self publishPoint:CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds))];
}

- (void)continueDynamicTouchAtPoint:(CGPoint)point inView:(UIView *)view
{
	if (!self.dynamicOriginActive || !self.trackingTouch || (view == nil))
	{
		return;
	}
	[self publishPoint:[view convertPoint:point toView:self]];
}

- (void)releaseTouch
{
	BOOL wasTracking = self.trackingTouch;
	self.accessibilitySteeringGeneration += 1;
	self.trackingTouch = NO;
	self.accessibilitySteering = NO;
	self.accessibilityValue = @"Centered";
	self.knob.center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
	if (self.dynamicOriginActive)
	{
		self.center = self.restingCenter;
		self.dynamicOriginActive = NO;
	}
	[self updateVisualAppearance];
	if (self.directionMask != 0)
	{
		Platform_InputTouchButton(self.directionMask, 0);
		self.directionMask = 0;
	}
	Platform_InputTouchLeftStick(0, 0, 0);
	if (wasTracking && (self.releaseHandler != nil))
	{
		self.releaseHandler();
	}
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)touches;
	(void)event;
	[self releaseTouch];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)touches;
	(void)event;
	[self releaseTouch];
}

@end

@implementation CTRPadTouchSettingsViewController

- (UIView *)cardWithViews:(NSArray<UIView *> *)views
{
    UIStackView *card = [[UIStackView alloc] initWithArrangedSubviews:views];
    card.axis = UILayoutConstraintAxisVertical;
    card.spacing = 14;
    card.layoutMarginsRelativeArrangement = YES;
    card.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(18, 18, 18, 18);
    card.backgroundColor = [UIColor colorWithWhite:1 alpha:0.055];
    card.layer.cornerRadius = 18;
    card.layer.cornerCurve = kCACornerCurveContinuous;
    return card;
}

- (void)exportDiagnostics:(UIButton *)sender
{
    sender.enabled = NO;
    struct utsname hardware;
    uname(&hardware);
    NSDictionary *metadata = @{
        @"Export time (UTC)": [[NSISO8601DateFormatter new] stringFromDate:NSDate.date],
        @"App version": [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"unknown",
        @"Build": [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleVersion"] ?: @"unknown",
        @"Source commit": [NSBundle.mainBundle objectForInfoDictionaryKey:@"CTRNativeSourceCommit"] ?: @"unknown",
        @"OS": UIDevice.currentDevice.systemVersion,
        @"Hardware model": [NSString stringWithUTF8String:hardware.machine],
        @"Thermal state (0 nominal, 3 critical)": @(NSProcessInfo.processInfo.thermalState),
        @"Touch enabled": @(self.touchEnabledSwitch.on),
        @"Classic controls": @(self.classicControlsSwitch.on),
        @"Steer right": @(self.handednessControl.selectedSegmentIndex == 1),
        @"Touch size (0 small, 2 large)": @(self.sizeControl.selectedSegmentIndex),
        @"Touch opacity": @(self.opacitySlider.value),
        @"Resolution scale": @(self.internalResolutionControl.selectedSegmentIndex + 1)
    };
    Platform_Log("[CTR Diagnostics] user requested export\n");
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString *report = CTRPadDiagnosticsReport(metadata);
        NSURL *directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:
            [@"CTRPad-diagnostics-" stringByAppendingString:NSUUID.UUID.UUIDString]] isDirectory:YES];
        NSError *error = nil;
        BOOL created = report && [NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error];
        NSURL *file = [directory URLByAppendingPathComponent:@"CTRPad-diagnostics.txt"];
        BOOL written = created && [report writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:&error];
        dispatch_async(dispatch_get_main_queue(), ^{
            sender.enabled = YES;
            if (!written)
            {
                [NSFileManager.defaultManager removeItemAtURL:directory error:nil];
                UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Could not export logs"
                    message:@"There may not be enough free storage. Please try again." preferredStyle:UIAlertControllerStyleAlert];
                [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
                [self presentViewController:alert animated:YES completion:nil];
                return;
            }
            UIActivityViewController *share = [[UIActivityViewController alloc] initWithActivityItems:@[file] applicationActivities:nil];
            share.popoverPresentationController.sourceView = sender;
            share.popoverPresentationController.sourceRect = sender.bounds;
            share.completionWithItemsHandler = ^(UIActivityType activity, BOOL completed, NSArray *items, NSError *shareError) {
                [NSFileManager.defaultManager removeItemAtURL:directory error:nil];
            };
            [self presentViewController:share animated:YES completion:nil];
        });
    });
}

- (UILabel *)sectionLabelWithText:(NSString *)text
{
	UILabel *label = [[UILabel alloc] init];
	label.text = text;
	label.textColor = [UIColor colorWithWhite:0.86 alpha:1.0];
	label.font = [UIFont systemFontOfSize:15.0 weight:UIFontWeightSemibold];
	return label;
}

- (void)styleSegmentedControl:(UISegmentedControl *)control
{
	control.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.08];
	control.selectedSegmentTintColor = [UIColor colorWithWhite:0.94 alpha:1.0];
	[control setTitleTextAttributes:@{ NSForegroundColorAttributeName : [UIColor colorWithWhite:0.82 alpha:1.0] }
	                       forState:UIControlStateNormal];
	[control setTitleTextAttributes:@{ NSForegroundColorAttributeName : [UIColor colorWithWhite:0.05 alpha:1.0] }
	                       forState:UIControlStateSelected];
}

- (void)viewDidLoad
{
	[super viewDidLoad];
	self.view.backgroundColor = [UIColor colorWithRed:0.025 green:0.035 blue:0.075 alpha:1.0];
	self.preferredContentSize = CGSizeMake(560.0, 680.0);
	self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

	UILabel *titleLabel = [[UILabel alloc] init];
	titleLabel.text = @"Options";
	titleLabel.textColor = UIColor.whiteColor;
	titleLabel.font = [UIFont systemFontOfSize:28.0 weight:UIFontWeightBold];
	titleLabel.textAlignment = NSTextAlignmentCenter;

	UILabel *bodyLabel = [[UILabel alloc] init];
	bodyLabel.text = @"Changes save automatically. The game pauses while you adjust options.";
	bodyLabel.textColor = [UIColor colorWithWhite:0.72 alpha:1.0];
	bodyLabel.font = [UIFont systemFontOfSize:15.0 weight:UIFontWeightRegular];
	bodyLabel.textAlignment = NSTextAlignmentCenter;
	bodyLabel.numberOfLines = 0;

	UILabel *touchEnabledLabel = [self sectionLabelWithText:@"On-screen controls"];
	self.touchEnabledSwitch = [[UISwitch alloc] init];
	self.touchEnabledSwitch.on = CTRPadTouch_ReadPreference(s_touchEnabledKey, 1, 1) != 0;
	self.touchEnabledSwitch.accessibilityIdentifier = @"ctrpad.touch.settings.enabled";
	self.touchEnabledSwitch.accessibilityLabel = @"On-screen touch controls";
	[self.touchEnabledSwitch addTarget:self action:@selector(preferencesChanged) forControlEvents:UIControlEventValueChanged];
	UIStackView *touchEnabledRow = [[UIStackView alloc] initWithArrangedSubviews:@[ touchEnabledLabel, self.touchEnabledSwitch ]];
	touchEnabledRow.axis = UILayoutConstraintAxisHorizontal;
	touchEnabledRow.alignment = UIStackViewAlignmentCenter;
	touchEnabledRow.distribution = UIStackViewDistributionEqualSpacing;

	UILabel *classicControlsLabel = [self sectionLabelWithText:@"Classic controls"];
	self.classicControlsSwitch = [[UISwitch alloc] init];
	self.classicControlsSwitch.on = CTRPadTouch_ReadPreference(s_simplifiedRacingControlsKey, 1, 1) == 0;
	self.classicControlsSwitch.accessibilityIdentifier = @"ctrpad.touch.settings.classic-controls";
	self.classicControlsSwitch.accessibilityLabel = @"Classic circular controls";
	[self.classicControlsSwitch addTarget:self action:@selector(preferencesChanged) forControlEvents:UIControlEventValueChanged];
	UIStackView *classicControlsRow = [[UIStackView alloc] initWithArrangedSubviews:@[ classicControlsLabel, self.classicControlsSwitch ]];
	classicControlsRow.axis = UILayoutConstraintAxisHorizontal;
	classicControlsRow.alignment = UIStackViewAlignmentCenter;
	classicControlsRow.distribution = UIStackViewDistributionEqualSpacing;

	UILabel *classicControlsHelp = [[UILabel alloc] init];
	classicControlsHelp.text = @"Restores the circular stick and physically held L/R Drift buttons.";
	classicControlsHelp.textColor = [UIColor colorWithWhite:0.68 alpha:1.0];
	classicControlsHelp.font = [UIFont systemFontOfSize:13.0 weight:UIFontWeightRegular];
	classicControlsHelp.numberOfLines = 0;

	self.handednessControl = [[UISegmentedControl alloc] initWithItems:@[ @"Steer left", @"Steer right" ]];
	self.handednessControl.selectedSegmentIndex = CTRPadTouch_ReadPreference(s_touchHandednessKey,
	                                                                        CTRPadTouchHandednessSteerLeft,
	                                                                        CTRPadTouchHandednessSteerRight);
	self.handednessControl.accessibilityIdentifier = @"ctrpad.touch.settings.handedness";
	[self.handednessControl addTarget:self action:@selector(preferencesChanged) forControlEvents:UIControlEventValueChanged];
	[self styleSegmentedControl:self.handednessControl];

	self.sizeControl = [[UISegmentedControl alloc] initWithItems:@[ @"Small", @"Standard", @"Large" ]];
	self.sizeControl.selectedSegmentIndex = CTRPadTouch_ReadPreference(s_touchSizeKey, CTRPadTouch_DefaultSize(), CTRPadTouchSizeLarge);
	self.sizeControl.accessibilityIdentifier = @"ctrpad.touch.settings.size";
	[self.sizeControl addTarget:self action:@selector(preferencesChanged) forControlEvents:UIControlEventValueChanged];
	[self styleSegmentedControl:self.sizeControl];

	self.opacityValueLabel = [self sectionLabelWithText:@""];
	self.opacityValueLabel.textAlignment = NSTextAlignmentRight;
	UIStackView *opacityTitleRow = [[UIStackView alloc] initWithArrangedSubviews:@[
		[self sectionLabelWithText:@"Control opacity"], self.opacityValueLabel
	]];
	opacityTitleRow.axis = UILayoutConstraintAxisHorizontal;
	opacityTitleRow.alignment = UIStackViewAlignmentCenter;
	opacityTitleRow.distribution = UIStackViewDistributionEqualSpacing;

	self.opacitySlider = [[UISlider alloc] init];
	self.opacitySlider.minimumValue = (float)s_touchOpacityMinimum;
	self.opacitySlider.maximumValue = (float)s_touchOpacityMaximum;
	self.opacitySlider.value = (float)CTRPadTouch_ReadOpacity();
	self.opacitySlider.continuous = YES;
	self.opacitySlider.minimumTrackTintColor = [UIColor colorWithRed:0.13 green:0.42 blue:0.84 alpha:1.0];
	self.opacitySlider.maximumTrackTintColor = [UIColor colorWithWhite:1.0 alpha:0.18];
	self.opacitySlider.accessibilityIdentifier = @"ctrpad.touch.settings.opacity";
	self.opacitySlider.accessibilityLabel = @"Control opacity";
	[self.opacitySlider addTarget:self action:@selector(opacityChanged) forControlEvents:UIControlEventValueChanged];
	[self updateOpacityValueLabel];

	self.internalResolutionControl = [[UISegmentedControl alloc] initWithItems:@[ @"1×", @"2×", @"3×", @"4×" ]];
	self.internalResolutionControl.selectedSegmentIndex = CTRPadTouch_ReadPreference(s_internalResolutionScaleKey, 0, 3);
	self.internalResolutionControl.accessibilityIdentifier = @"ctrpad.options.internal-resolution";
	self.internalResolutionControl.accessibilityLabel = @"Internal resolution";
	[self.internalResolutionControl addTarget:self action:@selector(preferencesChanged) forControlEvents:UIControlEventValueChanged];
	[self styleSegmentedControl:self.internalResolutionControl];

	UILabel *resolutionHelp = [[UILabel alloc] init];
	resolutionHelp.text = @"Higher values clean up polygon edges while keeping textures sharp.";
	resolutionHelp.textColor = [UIColor colorWithWhite:0.68 alpha:1.0];
	resolutionHelp.font = [UIFont systemFontOfSize:13.0 weight:UIFontWeightRegular];
	resolutionHelp.numberOfLines = 0;

	UIButtonConfiguration *resetConfiguration = [UIButtonConfiguration tintedButtonConfiguration];
	resetConfiguration.title = @"Reset defaults";
	resetConfiguration.baseForegroundColor = [UIColor colorWithRed:1.0 green:0.66 blue:0.19 alpha:1.0];
	UIButton *resetButton = [UIButton buttonWithConfiguration:resetConfiguration primaryAction:nil];
	resetButton.accessibilityIdentifier = @"ctrpad.touch.settings.reset";
	[resetButton addTarget:self action:@selector(resetDefaults) forControlEvents:UIControlEventTouchUpInside];

	UIButtonConfiguration *editConfiguration = [UIButtonConfiguration filledButtonConfiguration];
	editConfiguration.title = @"Edit touch layout";
	editConfiguration.baseForegroundColor = UIColor.whiteColor;
	editConfiguration.baseBackgroundColor = [UIColor colorWithRed:0.13 green:0.42 blue:0.84 alpha:1.0];
	self.editLayoutButton = [UIButton buttonWithConfiguration:editConfiguration primaryAction:nil];
	self.editLayoutButton.accessibilityIdentifier = @"ctrpad.touch.settings.edit-layout";
	self.editLayoutButton.enabled = self.touchEnabledSwitch.on;
	[self.editLayoutButton addTarget:self action:@selector(editLayout) forControlEvents:UIControlEventTouchUpInside];

	UIButtonConfiguration *discConfiguration = [UIButtonConfiguration tintedButtonConfiguration];
	discConfiguration.title = @"Change disc…";
	discConfiguration.baseForegroundColor = UIColor.whiteColor;
	UIButton *changeDiscButton = [UIButton buttonWithConfiguration:discConfiguration primaryAction:nil];
	changeDiscButton.accessibilityIdentifier = @"ctrpad.options.change-disc";
	changeDiscButton.accessibilityLabel = @"Change retail disc image";
	[changeDiscButton addTarget:self action:@selector(changeDisc) forControlEvents:UIControlEventTouchUpInside];

	UIButtonConfiguration *doneConfiguration = [UIButtonConfiguration filledButtonConfiguration];
	doneConfiguration.title = @"Done";
	doneConfiguration.baseBackgroundColor = [UIColor colorWithRed:0.13 green:0.42 blue:0.84 alpha:1.0];
	UIButton *doneButton = [UIButton buttonWithConfiguration:doneConfiguration primaryAction:nil];
	doneButton.accessibilityIdentifier = @"ctrpad.touch.settings.done";
	[doneButton addTarget:self action:@selector(done) forControlEvents:UIControlEventTouchUpInside];

    UIButtonConfiguration *exportConfiguration = [UIButtonConfiguration filledButtonConfiguration];
    exportConfiguration.title = @"Export diagnostic logs";
    exportConfiguration.image = [UIImage systemImageNamed:@"square.and.arrow.up"];
    exportConfiguration.imagePadding = 10;
    exportConfiguration.baseBackgroundColor = [UIColor colorWithRed:0.12 green:0.38 blue:0.80 alpha:1];
    UIButton *exportButton = [UIButton buttonWithConfiguration:exportConfiguration primaryAction:nil];
    exportButton.accessibilityIdentifier = @"ctrpad.options.export-logs";
    [exportButton.heightAnchor constraintGreaterThanOrEqualToConstant:50].active = YES;
    [exportButton addTarget:self action:@selector(exportDiagnostics:) forControlEvents:UIControlEventTouchUpInside];
    UILabel *diagnosticsHelp = [self sectionLabelWithText:@"After a crash, reopen CTRPad and export here. Includes recent logs and build details; excludes saves and game data. Review the text before sharing. Nothing is uploaded automatically."];
    diagnosticsHelp.numberOfLines = 0;
    diagnosticsHelp.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    UILabel *buildLabel = [self sectionLabelWithText:[NSString stringWithFormat:@"CTRPad %@ · Build %@",
        [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"?",
        [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleVersion"] ?: @"?"]];
    buildLabel.font = [UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightRegular];

	UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[

        [self cardWithViews:@[ [self sectionLabelWithText:@"TOUCH CONTROLS"], touchEnabledRow, classicControlsRow,
            classicControlsHelp, self.editLayoutButton, [self sectionLabelWithText:@"Handedness"], self.handednessControl,
            [self sectionLabelWithText:@"Control size"], self.sizeControl, opacityTitleRow, self.opacitySlider ]],
        [self cardWithViews:@[ [self sectionLabelWithText:@"DIAGNOSTICS"], exportButton, diagnosticsHelp, buildLabel ]],
        [self cardWithViews:@[ [self sectionLabelWithText:@"DISPLAY"], self.internalResolutionControl, resolutionHelp ]],
        changeDiscButton,
        resetButton,
	]];
	stack.translatesAutoresizingMaskIntoConstraints = NO;
	stack.axis = UILayoutConstraintAxisVertical;
	stack.alignment = UIStackViewAlignmentFill;
	stack.spacing = 12.0;

	[stack setCustomSpacing:22.0 afterView:changeDiscButton];


	UIScrollView *scrollView = [[UIScrollView alloc] init];
	scrollView.translatesAutoresizingMaskIntoConstraints = NO;
	scrollView.alwaysBounceVertical = NO;
	[self.view addSubview:scrollView];
	[scrollView addSubview:stack];

    UIStackView *heading = [[UIStackView alloc] initWithArrangedSubviews:@[titleLabel, doneButton]];
    heading.axis = UILayoutConstraintAxisHorizontal;
    heading.alignment = UIStackViewAlignmentCenter;
    heading.spacing = 16;
    titleLabel.textAlignment = NSTextAlignmentLeft;
    [doneButton.widthAnchor constraintEqualToConstant:88].active = YES;
    UIStackView *header = [[UIStackView alloc] initWithArrangedSubviews:@[heading, bodyLabel]];
    header.axis = UILayoutConstraintAxisVertical;
    header.spacing = 8;
    header.translatesAutoresizingMaskIntoConstraints = NO;
    bodyLabel.textAlignment = NSTextAlignmentLeft;
    [self.view addSubview:header];
	UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
	[NSLayoutConstraint activateConstraints:@[
        [header.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:24],
        [header.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-24],
        [header.topAnchor constraintEqualToAnchor:safe.topAnchor constant:16],
		[scrollView.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
		[scrollView.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
		[scrollView.topAnchor constraintEqualToAnchor:header.bottomAnchor constant:12],
		[scrollView.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],
		[stack.leadingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor constant:24.0],
		[stack.trailingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor constant:-24.0],
		[stack.topAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor constant:24.0],
		[stack.bottomAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor constant:-24.0],
		[scrollView.contentLayoutGuide.widthAnchor constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor],
		[self.handednessControl.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[self.sizeControl.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[self.opacitySlider.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[self.internalResolutionControl.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[self.classicControlsSwitch.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[self.editLayoutButton.heightAnchor constraintGreaterThanOrEqualToConstant:48.0],
		[changeDiscButton.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[resetButton.heightAnchor constraintGreaterThanOrEqualToConstant:44.0],
		[doneButton.heightAnchor constraintGreaterThanOrEqualToConstant:50.0],
	]];
}

- (void)updateOpacityValueLabel
{
	NSString *value = [NSString stringWithFormat:@"%.0f%%", self.opacitySlider.value * 100.0f];
	self.opacityValueLabel.text = value;
	self.opacitySlider.accessibilityValue = value;
}

- (void)opacityChanged
{
	[NSUserDefaults.standardUserDefaults setDouble:self.opacitySlider.value forKey:s_touchOpacityKey];
	[self updateOpacityValueLabel];
}

- (void)preferencesChanged
{
	NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
	[defaults setInteger:self.handednessControl.selectedSegmentIndex forKey:s_touchHandednessKey];
	[defaults setInteger:self.sizeControl.selectedSegmentIndex forKey:s_touchSizeKey];
	NSInteger requestedScale = self.internalResolutionControl.selectedSegmentIndex + 1;
	NSInteger appliedScale = NativeRenderer_SetInternalResolutionScale((int)requestedScale);
	self.internalResolutionControl.selectedSegmentIndex = appliedScale - 1;
	[defaults setInteger:self.internalResolutionControl.selectedSegmentIndex forKey:s_internalResolutionScaleKey];
	[defaults setBool:self.touchEnabledSwitch.on forKey:s_touchEnabledKey];
	[defaults setBool:!self.classicControlsSwitch.on forKey:s_simplifiedRacingControlsKey];
	self.editLayoutButton.enabled = self.touchEnabledSwitch.on;
	[self.overlayController applyPreferencesAndRebuildControls];
}

- (void)resetDefaults
{
	NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
	[defaults removeObjectForKey:s_touchHandednessKey];
	[defaults removeObjectForKey:s_touchSizeKey];
	[defaults removeObjectForKey:s_touchOpacityKey];
	[defaults removeObjectForKey:s_touchEnabledKey];
	[defaults removeObjectForKey:s_simplifiedRacingControlsKey];
	[defaults removeObjectForKey:s_internalResolutionScaleKey];
	self.handednessControl.selectedSegmentIndex = CTRPadTouchHandednessSteerLeft;
	self.sizeControl.selectedSegmentIndex = CTRPadTouch_DefaultSize();
	self.opacitySlider.value = (float)CTRPadTouch_DefaultOpacity();
	[self updateOpacityValueLabel];
	self.touchEnabledSwitch.on = YES;
	self.classicControlsSwitch.on = NO;
	self.internalResolutionControl.selectedSegmentIndex = 0;
	NativeRenderer_SetInternalResolutionScale(1);
	self.editLayoutButton.enabled = YES;
	[self.overlayController resetAllLayouts];
	[self.overlayController applyPreferencesAndRebuildControls];
	UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, @"Options reset to defaults.");
}

- (void)changeDisc
{
	[self.overlayController resetControlState];
	__weak CTRPadTouchOverlayViewController *overlay = self.overlayController;
	[self dismissViewControllerAnimated:YES completion:^{
		[overlay presentDiscReselectionConfirmation];
	}];
}

- (void)editLayout
{
	if (!self.touchEnabledSwitch.on)
	{
		return;
	}
	[self.overlayController resetControlState];
	__weak CTRPadTouchOverlayViewController *overlay = self.overlayController;
	[self dismissViewControllerAnimated:YES completion:^{
		[overlay beginLayoutEditing];
	}];
}

- (void)done
{
	[self.overlayController resetControlState];
	[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
	[self.overlayController resetControlState];
	[self.overlayController applyPreferencesAndRebuildControls];
}

@end

@implementation CTRPadTouchOverlayViewController

- (void)loadView
{
	self.view = [[CTRPadTouchPassthroughView alloc] initWithFrame:CGRectZero];
	self.view.backgroundColor = UIColor.clearColor;
	self.view.multipleTouchEnabled = YES;
	self.view.accessibilityViewIsModal = NO;
}

- (CTRPadInputButton *)buttonWithTitle:(NSString *)title mask:(enum PlatformInputTouchButton)mask color:(UIColor *)color
{
	CTRPadInputButton *button = [CTRPadInputButton buttonWithType:UIButtonTypeCustom];
	button.translatesAutoresizingMaskIntoConstraints = NO;
	button.tag = mask;
	button.exclusiveTouch = NO;
	button.baseColor = color;
	button.controlOpacity = self.controlOpacity;
	button.releasedTitle = title;
	button.layer.borderWidth = 1.0;
	button.layer.cornerCurve = kCACornerCurveContinuous;
	button.layer.shadowColor = UIColor.blackColor.CGColor;
	button.layer.shadowOpacity = 0.25;
	button.layer.shadowRadius = 4;
	button.layer.shadowOffset = CGSizeMake(0, 2);
	button.titleLabel.font = [UIFont systemFontOfSize:14.0 weight:UIFontWeightBold];
	button.titleLabel.numberOfLines = 2;
	button.titleLabel.textAlignment = NSTextAlignmentCenter;
	[button setTitle:title forState:UIControlStateNormal];
	[button updateAppearance];
	button.accessibilityValue = @"Released";
	button.accessibilityCustomActions = @[
		[[UIAccessibilityCustomAction alloc] initWithName:@"Press one second" target:button selector:@selector(accessibilityPressBriefly)],
		[[UIAccessibilityCustomAction alloc] initWithName:@"Press three seconds" target:button selector:@selector(accessibilityPressThreeSeconds)],
		[[UIAccessibilityCustomAction alloc] initWithName:@"Hold" target:button selector:@selector(accessibilityHoldControl)],
		[[UIAccessibilityCustomAction alloc] initWithName:@"Release" target:button selector:@selector(accessibilityReleaseControl)],
	];
	[button addTarget:self action:@selector(buttonDown:) forControlEvents:UIControlEventTouchDown | UIControlEventTouchDragEnter];
	[button addTarget:self
	              action:@selector(buttonUp:)
	    forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel | UIControlEventTouchDragExit];
	return button;
}

- (UIButton *)utilityButtonWithTitle:(NSString *)title action:(SEL)action
{
	UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
	CGFloat relativeOpacity = CTRPadTouch_RelativeOpacity(self.controlOpacity);
	button.translatesAutoresizingMaskIntoConstraints = NO;
	button.exclusiveTouch = YES;
	button.backgroundColor = [[UIColor colorWithWhite:0.08 alpha:1.0] colorWithAlphaComponent:self.controlOpacity];
	button.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:MIN(1.0, 0.68 * relativeOpacity)].CGColor;
	button.layer.borderWidth = 1.0;
	button.layer.cornerCurve = kCACornerCurveContinuous;
	button.layer.shadowColor = UIColor.blackColor.CGColor;
	button.layer.shadowOpacity = 0.25;
	button.layer.shadowRadius = 4;
	button.layer.shadowOffset = CGSizeMake(0, 2);
	button.layer.cornerRadius = 12.0;
	button.titleLabel.font = [UIFont systemFontOfSize:12.0 weight:UIFontWeightBold];
	[button setTitle:title forState:UIControlStateNormal];
	[button setTitleColor:[UIColor colorWithWhite:1.0 alpha:MIN(1.0, relativeOpacity)] forState:UIControlStateNormal];
	[button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
	return button;
}

- (void)buttonDown:(UIButton *)sender
{
	CTRPadInputButton *button = [sender isKindOfClass:CTRPadInputButton.class] ? (CTRPadInputButton *)sender : nil;
	if ((button == nil) || button.layoutEditing)
	{
		return;
	}
	if (button.inputLatched)
	{
		button.latchGeneration += 1;
		button.inputLatched = NO;
		button.inputPressed = NO;
		button.accessibilityValue = @"Released";
		[button stopHoldProgress];
		[button updateAppearance];
		Platform_InputTouchButton((unsigned int)button.tag, 0);
		if (button.latchReleasedAnnouncement.length > 0)
		{
			UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, button.latchReleasedAnnouncement);
		}
		return;
	}
	if (button.inputPressed)
	{
		return;
	}

	button.inputPressed = YES;
	button.accessibilityValue = @"Held";
	[button updateAppearance];
	Platform_InputTouchButton((unsigned int)button.tag, 1);
	if (button.latchOnTap)
	{
		button.inputPressed = NO;
		button.inputLatched = YES;
		button.accessibilityValue = @"Active";
		[button updateAppearance];
		UIImpactFeedbackGenerator *feedback = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
		[feedback impactOccurred];
		if (button.latchEngagedAnnouncement.length > 0)
		{
			UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, button.latchEngagedAnnouncement);
		}
		return;
	}
	if (button.holdToLatch)
	{
		[button beginHoldProgress];
		NSUInteger generation = ++button.latchGeneration;
		dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(s_gasLatchDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
			if ((button.latchGeneration == generation) && button.inputPressed && !button.layoutEditing)
			{
				button.inputLatched = YES;
				button.accessibilityValue = @"Gas locked";
				[button stopHoldProgress];
				[button updateAppearance];
				UIImpactFeedbackGenerator *feedback = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
				[feedback impactOccurred];
				UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, @"Gas locked. Tap Gas to release.");
			}
		});
	}
}

- (void)buttonUp:(UIButton *)sender
{
	if (![sender isKindOfClass:CTRPadInputButton.class])
	{
		return;
	}
	CTRPadInputButton *button = (CTRPadInputButton *)sender;
	button.latchGeneration += 1;
	[button stopHoldProgress];
	button.accessibilityHeld = NO;
	button.accessibilityTapPending = NO;
	if (button.layoutEditing || button.inputLatched || !button.inputPressed)
	{
		return;
	}
	button.inputPressed = NO;
	button.accessibilityValue = @"Released";
	[button updateAppearance];
	Platform_InputTouchButton((unsigned int)button.tag, 0);
}

- (void)releaseLatchedDrift
{
	CTRPadInputButton *button = self.driftButton;
	if (!self.simplifiedRacingControls || (button == nil) || !button.inputLatched)
	{
		return;
	}
	button.latchGeneration += 1;
	button.inputLatched = NO;
	button.inputPressed = NO;
	button.accessibilityValue = @"Released";
	[button updateAppearance];
	Platform_InputTouchButton((unsigned int)button.tag, 0);
	if (button.latchReleasedAnnouncement.length > 0)
	{
		UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, button.latchReleasedAnnouncement);
	}
}

- (void)resetControlState
{
	for (UIView *control in self.view.subviews)
	{
		if ([control isKindOfClass:CTRPadInputButton.class])
		{
			CTRPadInputButton *button = (CTRPadInputButton *)control;
			[button cancelInput];
		}
		else if ([control isKindOfClass:CTRPadTouchStickView.class])
		{
			[(CTRPadTouchStickView *)control releaseTouch];
		}
	}
	Platform_InputTouchReset();
}

- (void)presentTouchSettings
{
	if (self.presentedViewController != nil)
	{
		return;
	}

	[self resetControlState];
	CTRPadTouchSettingsViewController *settings = [[CTRPadTouchSettingsViewController alloc] init];
	settings.overlayController = self;
	settings.modalPresentationStyle = UIModalPresentationFormSheet;
	[self presentViewController:settings animated:YES completion:nil];
}

- (void)presentDiscReselectionConfirmation
{
	if ((self.presentedViewController != nil) || (s_discReselectionCallback == NULL))
	{
		return;
	}
	[self resetControlState];

	UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Choose a different disc?"
	                                                              message:@"CTRPad must stop the current game before opening Files. Memory-card saves are kept, but unsaved race progress will be lost."
	                                                       preferredStyle:UIAlertControllerStyleAlert];
	[alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
	[alert addAction:[UIAlertAction actionWithTitle:@"Stop Game & Choose"
	                                         style:UIAlertActionStyleDefault
	                                       handler:^(__unused UIAlertAction *action) {
	                                         // Allow the alert dismissal transition to finish before the
	                                         // display callback removes the game view hierarchy.
	                                         dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.35 * NSEC_PER_SEC)),
	                                                        dispatch_get_main_queue(), ^{
	                                                          if (s_discReselectionCallback != NULL)
	                                                          {
		                                                          s_discReselectionCallback(s_discReselectionUserdata);
	                                                          }
	                                                        });
	                                       }]];
	alert.preferredAction = alert.actions.lastObject;
	[self presentViewController:alert animated:YES completion:nil];
}

- (void)viewDidLoad
{
	[super viewDidLoad];
	NSNotificationCenter *notifications = NSNotificationCenter.defaultCenter;
	[notifications addObserver:self selector:@selector(lifecycleWillSuspend:) name:UIApplicationWillResignActiveNotification object:nil];
	[notifications addObserver:self selector:@selector(lifecycleWillSuspend:) name:UIApplicationDidEnterBackgroundNotification object:nil];
	[self applyPreferencesAndRebuildControls];
}

- (void)dealloc
{
	[NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)lifecycleWillSuspend:(NSNotification *)notification
{
	(void)notification;
	[self resetControlState];
}

- (void)viewDidLayoutSubviews
{
	[super viewDidLayoutSubviews];
	[self layoutControls];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
	[self resetControlState];
	[super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
}

- (void)applyPreferencesAndRebuildControls
{
	[self resetControlState];
	for (UIView *subview in self.view.subviews.copy)
	{
		[subview removeFromSuperview];
	}

	self.handedness = (CTRPadTouchHandedness)CTRPadTouch_ReadPreference(s_touchHandednessKey,
	                                                                  CTRPadTouchHandednessSteerLeft,
	                                                                  CTRPadTouchHandednessSteerRight);
	CTRPadTouchSize sizeChoice = (CTRPadTouchSize)CTRPadTouch_ReadPreference(s_touchSizeKey,
	                                                                       CTRPadTouch_DefaultSize(),
	                                                                       CTRPadTouchSizeLarge);
	self.controlScale = CTRPadTouch_ScaleForChoice(sizeChoice);
	self.controlOpacity = CTRPadTouch_ReadOpacity();
	self.touchControlsEnabled = CTRPadTouch_ReadPreference(s_touchEnabledKey, 1, 1) != 0;
	self.simplifiedRacingControls = CTRPadTouch_ReadPreference(s_simplifiedRacingControlsKey, 1, 1) != 0;
	BOOL phone = UIDevice.currentDevice.userInterfaceIdiom != UIUserInterfaceIdiomPad;
	NSInteger requestedResolutionScale = CTRPadTouch_ReadPreference(s_internalResolutionScaleKey, 0, 3) + 1;
	NSInteger appliedResolutionScale = NativeRenderer_SetInternalResolutionScale((int)requestedResolutionScale);
	if (appliedResolutionScale != requestedResolutionScale)
	{
		[NSUserDefaults.standardUserDefaults setInteger:appliedResolutionScale - 1 forKey:s_internalResolutionScaleKey];
	}

	CTRPadTouchStickView *stick = [[CTRPadTouchStickView alloc] init];
	stick.horizontalOnly = self.simplifiedRacingControls;
	if (stick.horizontalOnly)
	{
		stick.accessibilityLabel = @"Horizontal steering slider";
		stick.accessibilityHint = @"Move left or right to steer. Vertical movement still navigates menus.";
	}
	[stick applyControlOpacity:self.controlOpacity];
	CTRPadTouchSteeringZoneView *steeringZone = [[CTRPadTouchSteeringZoneView alloc] initWithFrame:CGRectZero];
	steeringZone.multipleTouchEnabled = NO;
	steeringZone.backgroundColor = UIColor.clearColor;
	steeringZone.stick = stick;
	steeringZone.hidden = !self.touchControlsEnabled;
	steeringZone.userInteractionEnabled = self.touchControlsEnabled;
	self.steeringZone = steeringZone;
	[self.view addSubview:steeringZone];
	CTRPadInputButton *cross = [self buttonWithTitle:@"GAS\n✕" mask:PLATFORM_INPUT_TOUCH_CROSS color:[UIColor colorWithRed:0.08 green:0.42 blue:0.95 alpha:1.0]];
	CTRPadInputButton *square = [self buttonWithTitle:@"BRAKE\n□" mask:PLATFORM_INPUT_TOUCH_SQUARE color:[UIColor colorWithRed:0.93 green:0.19 blue:0.52 alpha:1.0]];
	CTRPadInputButton *circle = [self buttonWithTitle:@"ITEM\n○" mask:PLATFORM_INPUT_TOUCH_CIRCLE color:[UIColor colorWithRed:0.93 green:0.16 blue:0.18 alpha:1.0]];
	CTRPadInputButton *triangle = [self buttonWithTitle:@"VIEW\n△" mask:PLATFORM_INPUT_TOUCH_TRIANGLE color:[UIColor colorWithRed:0.08 green:0.68 blue:0.32 alpha:1.0]];
	NSString *leftDriftTitle = self.simplifiedRacingControls ? @"BOOST\nTAP" : (phone ? @"BOOST\nTAP" : @"L DRIFT / BOOST");
	NSString *rightDriftTitle = self.simplifiedRacingControls ? @"DRIFT\nTOGGLE" : (phone ? @"DRIFT\nHOLD" : @"R DRIFT / BOOST");
	CTRPadInputButton *leftDrift = [self buttonWithTitle:leftDriftTitle mask:PLATFORM_INPUT_TOUCH_L1 color:[UIColor colorWithRed:0.94 green:0.59 blue:0.10 alpha:1.0]];
	CTRPadInputButton *rightDrift = [self buttonWithTitle:rightDriftTitle mask:PLATFORM_INPUT_TOUCH_R1 color:[UIColor colorWithRed:0.94 green:0.59 blue:0.10 alpha:1.0]];
	CTRPadInputButton *start = [self buttonWithTitle:@"START\nPAUSE" mask:PLATFORM_INPUT_TOUCH_START color:[UIColor colorWithWhite:0.08 alpha:1.0]];
	CTRPadInputButton *select = [self buttonWithTitle:@"SELECT" mask:PLATFORM_INPUT_TOUCH_SELECT color:[UIColor colorWithWhite:0.08 alpha:1.0]];
	UIButton *settings = [self utilityButtonWithTitle:@"OPTIONS" action:@selector(presentTouchSettings)];
	cross.holdToLatch = YES;
	cross.latchedTitle = @"AUTO\nGAS";
	cross.latchEngagedAnnouncement = @"Gas locked. Tap Gas to release.";
	cross.latchReleasedAnnouncement = @"Gas lock released.";
	CAShapeLayer *gasProgress = [CAShapeLayer layer];
	gasProgress.fillColor = UIColor.clearColor.CGColor;
	gasProgress.strokeColor = UIColor.whiteColor.CGColor;
	gasProgress.lineWidth = 4.0;
	gasProgress.lineCap = kCALineCapRound;
	gasProgress.hidden = YES;
	cross.holdProgressLayer = gasProgress;
	[cross.layer addSublayer:gasProgress];
	if (self.simplifiedRacingControls)
	{
		rightDrift.latchOnTap = YES;
		rightDrift.latchedTitle = @"DRIFT\nACTIVE";
		rightDrift.latchEngagedAnnouncement = @"Drift active. Tap Boost in the red zone.";
		rightDrift.latchReleasedAnnouncement = @"Drift released.";
	}
	self.driftButton = rightDrift;
	__weak CTRPadTouchOverlayViewController *weakSelf = self;
	stick.releaseHandler = ^{
		[weakSelf releaseLatchedDrift];
	};

	cross.accessibilityIdentifier = @"ctrpad.touch.cross";
	square.accessibilityIdentifier = @"ctrpad.touch.square";
	circle.accessibilityIdentifier = @"ctrpad.touch.circle";
	triangle.accessibilityIdentifier = @"ctrpad.touch.triangle";
	leftDrift.accessibilityIdentifier = @"ctrpad.touch.l1";
	rightDrift.accessibilityIdentifier = @"ctrpad.touch.r1";
	start.accessibilityIdentifier = @"ctrpad.touch.start";
	start.accessibilityLabel = @"Start or pause";
	select.accessibilityIdentifier = @"ctrpad.touch.select";
	settings.accessibilityIdentifier = @"ctrpad.touch.settings";
	settings.accessibilityLabel = @"Open options";
	cross.accessibilityLabel = @"Gas, PlayStation Cross";
	square.accessibilityLabel = @"Brake, PlayStation Square";
	circle.accessibilityLabel = @"Item, PlayStation Circle";
	triangle.accessibilityLabel = @"View, PlayStation Triangle";
	leftDrift.accessibilityLabel = self.simplifiedRacingControls
	                                   ? @"Boost, tap while Drift is active, PlayStation L1"
	                                   : (phone ? @"Boost, tap while holding Drift, PlayStation L1" : @"Left drift or boost, PlayStation L1");
	rightDrift.accessibilityLabel = self.simplifiedRacingControls
	                                    ? @"Drift toggle, PlayStation R1"
	                                    : (phone ? @"Drift, hold while steering, PlayStation R1" : @"Right drift or boost, PlayStation R1");
	select.accessibilityLabel = @"PlayStation Select";

	NSArray<UIView *> *controls = @[ stick, cross, square, circle, triangle, leftDrift, rightDrift, start, select, settings ];
	for (UIView *control in controls)
	{
		control.translatesAutoresizingMaskIntoConstraints = YES;
		[self.view addSubview:control];
	}
	self.editableControls = @[ stick, cross, square, circle, triangle, leftDrift, rightDrift, start, select ];
	for (UIView *control in self.editableControls)
	{
		control.hidden = !self.touchControlsEnabled;
		control.userInteractionEnabled = self.touchControlsEnabled;
	}
	Platform_InputTouchSetEnabled(self.touchControlsEnabled ? 1 : 0);
	self.editGestures = [NSMutableArray array];
	self.layoutCenters = [NSMutableDictionary dictionary];
	self.layoutScales = [NSMutableDictionary dictionary];
	self.layoutProfile = nil;
	self.selectedControl = nil;
	[self installLayoutEditor];
	[self.view setNeedsLayout];
}

- (CGRect)usableBounds
{
	CGRect safe = UIEdgeInsetsInsetRect(self.view.bounds, self.view.safeAreaInsets);
	return CGRectInset(safe, 4.0, 4.0);
}

- (UIView *)controlWithIdentifier:(NSString *)identifier
{
	for (UIView *view in self.view.subviews)
	{
		if ([view.accessibilityIdentifier isEqualToString:identifier])
		{
			return view;
		}
	}
	return nil;
}

- (NSString *)currentLayoutProfile
{
	NSString *device = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad ? @"tablet" : @"phone";
	NSString *grip = self.handedness == CTRPadTouchHandednessSteerRight ? @"right" : @"left";
	NSString *mode = self.simplifiedRacingControls ? @"-simplified" : @"";
	return [NSString stringWithFormat:@"%@-%@%@-v1", device, grip, mode];
}

- (NSString *)layoutStorageKey
{
	return [NSString stringWithFormat:@"%@.%@", s_touchLayoutKeyPrefix, self.layoutProfile ?: [self currentLayoutProfile]];
}

- (void)loadCurrentLayoutIfNeeded
{
	NSString *profile = [self currentLayoutProfile];
	if ([self.layoutProfile isEqualToString:profile])
	{
		return;
	}
	self.layoutProfile = profile;
	[self.layoutCenters removeAllObjects];
	[self.layoutScales removeAllObjects];
	NSDictionary *stored = [NSUserDefaults.standardUserDefaults dictionaryForKey:[self layoutStorageKey]];
	if (stored == nil) stored = CTRPadTouch_DefaultLayout(profile);
	NSDictionary *centers = [stored isKindOfClass:NSDictionary.class] ? stored[@"centers"] : nil;
	if ([centers isKindOfClass:NSDictionary.class])
	{
		[centers enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, __unused BOOL *stop) {
			if ([key isKindOfClass:NSString.class] && [value isKindOfClass:NSArray.class] && [(NSArray *)value count] == 2)
			{
				self.layoutCenters[key] = value;
			}
		}];
	}
	NSDictionary *scales = [stored isKindOfClass:NSDictionary.class] ? stored[@"scales"] : nil;
	if ([scales isKindOfClass:NSDictionary.class])
	{
		[scales enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, __unused BOOL *stop) {
			CGFloat scale = [value isKindOfClass:NSNumber.class] ? [value doubleValue] : 0.0;
			if ([key isKindOfClass:NSString.class] && isfinite(scale) && (scale >= 0.70) && (scale <= 1.50))
			{
				self.layoutScales[key] = @(scale);
			}
		}];
	}
}

- (void)clampControlToUsableBounds:(UIView *)control
{
	CGRect safe = [self usableBounds];
	CGFloat halfWidth = CGRectGetWidth(control.bounds) * 0.5;
	CGFloat halfHeight = CGRectGetHeight(control.bounds) * 0.5;
	CGFloat minX = CGRectGetMinX(safe) + halfWidth;
	CGFloat maxX = CGRectGetMaxX(safe) - halfWidth;
	CGFloat minY = CGRectGetMinY(safe) + halfHeight;
	CGFloat maxY = CGRectGetMaxY(safe) - halfHeight;
	control.center = CGPointMake(MIN(MAX(control.center.x, minX), MAX(minX, maxX)),
	                             MIN(MAX(control.center.y, minY), MAX(minY, maxY)));
}

- (void)setControl:(UIView *)control normalizedCenter:(CGPoint)normalized size:(CGSize)size mirror:(BOOL)mirror
{
	if (control == nil)
	{
		return;
	}
	CGRect safe = [self usableBounds];
	if (mirror && (self.handedness == CTRPadTouchHandednessSteerRight))
	{
		normalized.x = 1.0 - normalized.x;
	}
	control.bounds = CGRectMake(0.0, 0.0, round(size.width), round(size.height));
	control.center = CGPointMake(CGRectGetMinX(safe) + normalized.x * CGRectGetWidth(safe),
	                             CGRectGetMinY(safe) + normalized.y * CGRectGetHeight(safe));
	NSArray<NSNumber *> *saved = self.layoutCenters[control.accessibilityIdentifier];
	if ([saved isKindOfClass:NSArray.class] && (saved.count == 2))
	{
		CGFloat x = saved[0].doubleValue;
		CGFloat y = saved[1].doubleValue;
		if (isfinite(x) && isfinite(y))
		{
			control.center = CGPointMake(CGRectGetMinX(safe) + x * CGRectGetWidth(safe),
			                             CGRectGetMinY(safe) + y * CGRectGetHeight(safe));
		}
	}
	[self clampControlToUsableBounds:control];
}

- (CGSize)sizeForControl:(NSString *)identifier baseSize:(CGSize)baseSize
{
	CGFloat individualScale = [self.layoutScales[identifier] doubleValue];
	if (!isfinite(individualScale) || (individualScale < 0.70) || (individualScale > 1.50))
		individualScale = 1.0;
	return CGSizeMake(baseSize.width * individualScale, baseSize.height * individualScale);
}

- (void)layoutControls
{
	if ((CGRectGetWidth(self.view.bounds) <= 0.0) || (CGRectGetHeight(self.view.bounds) <= 0.0) || (self.editableControls.count == 0))
	{
		return;
	}
	[self loadCurrentLayoutIfNeeded];
	BOOL phone = UIDevice.currentDevice.userInterfaceIdiom != UIUserInterfaceIdiomPad;
	CGRect safe = [self usableBounds];
	CGFloat scale = self.controlScale;
	CGSize stickBaseSize = self.simplifiedRacingControls ? CGSizeMake(206.0 * scale, 104.0 * scale)
	                                                    : CGSizeMake(174.0 * scale, 174.0 * scale);
	CGSize stickSize = [self sizeForControl:@"ctrpad.touch.stick" baseSize:stickBaseSize];
	CGSize driftSize = [self sizeForControl:@"ctrpad.touch.l1" baseSize:CGSizeMake(144.0 * scale, MAX(48.0, 54.0 * scale))];
	CGSize rightDriftSize = [self sizeForControl:@"ctrpad.touch.r1" baseSize:CGSizeMake(144.0 * scale, MAX(48.0, 54.0 * scale))];
	CGSize gasSize = [self sizeForControl:@"ctrpad.touch.cross" baseSize:CGSizeMake(92.0 * scale, 92.0 * scale)];
	CGSize brakeSize = [self sizeForControl:@"ctrpad.touch.square" baseSize:CGSizeMake(MAX(56.0, 64.0 * scale), MAX(56.0, 64.0 * scale))];
	CGSize itemSize = [self sizeForControl:@"ctrpad.touch.circle" baseSize:CGSizeMake(MAX(56.0, 64.0 * scale), MAX(56.0, 64.0 * scale))];
	CGSize viewSize = [self sizeForControl:@"ctrpad.touch.triangle" baseSize:CGSizeMake(MAX(56.0, 64.0 * scale), MAX(56.0, 64.0 * scale))];
	CGSize startSize = [self sizeForControl:@"ctrpad.touch.start" baseSize:CGSizeMake(82.0 * scale, MAX(44.0, 40.0 * scale))];
	CGSize selectSize = [self sizeForControl:@"ctrpad.touch.select" baseSize:CGSizeMake(82.0 * scale, MAX(44.0, 40.0 * scale))];

	CGFloat safeWidth = MAX(1.0, CGRectGetWidth(safe));
	CGFloat safeHeight = MAX(1.0, CGRectGetHeight(safe));
	CGPoint stickPoint = CGPointMake(CGRectGetMinX(safe) + 24.0 * scale + stickSize.width * 0.5,
	                                CGRectGetMaxY(safe) - 24.0 * scale - stickSize.height * 0.5);
	CGPoint gasPoint = CGPointMake(CGRectGetMaxX(safe) - 24.0 * scale - gasSize.width * 0.5,
	                              CGRectGetMaxY(safe) - 22.0 * scale - gasSize.height * 0.5);
	CGPoint brakePoint = CGPointMake(gasPoint.x - gasSize.width * 0.5 - 18.0 * scale - brakeSize.width * 0.5,
	                                CGRectGetMaxY(safe) - 34.0 * scale - brakeSize.height * 0.5);
	CGPoint itemPoint = CGPointMake(CGRectGetMaxX(safe) - 36.0 * scale - itemSize.width * 0.5,
	                               gasPoint.y - gasSize.height * 0.5 - 16.0 * scale - itemSize.height * 0.5);
	CGPoint viewPoint = CGPointMake(itemPoint.x - itemSize.width * 0.5 - 14.0 * scale - viewSize.width * 0.5,
	                               itemPoint.y);
	CGFloat utilityClusterX = viewPoint.x - viewSize.width * 0.5 - 14.0 * scale - MAX(startSize.width, selectSize.width) * 0.5;
	CGPoint startPoint = CGPointMake(utilityClusterX, viewPoint.y - startSize.height * 0.5 - 8.0 * scale);
	CGPoint selectPoint = CGPointMake(utilityClusterX, viewPoint.y + selectSize.height * 0.5 + 8.0 * scale);
	if (self.handedness == CTRPadTouchHandednessSteerRight)
	{
		CGPoint *mirroredPoints[] = { &stickPoint, &gasPoint, &brakePoint, &itemPoint, &viewPoint, &startPoint, &selectPoint };
		for (NSUInteger index = 0; index < sizeof(mirroredPoints) / sizeof(mirroredPoints[0]); ++index)
			mirroredPoints[index]->x = CGRectGetMinX(safe) + CGRectGetMaxX(safe) - mirroredPoints[index]->x;
	}
	CGFloat actionClusterTop = MIN(startPoint.y - startSize.height * 0.5,
	                               MIN(viewPoint.y - viewSize.height * 0.5, itemPoint.y - itemSize.height * 0.5));
	CGFloat driftGroupY = actionClusterTop - 12.0 * scale - MAX(driftSize.height, rightDriftSize.height) * 0.5;
	CGPoint rightDriftPoint = CGPointMake(CGRectGetMaxX(safe) - 18.0 * scale - rightDriftSize.width * 0.5,
	                                     driftGroupY);
	CGPoint leftDriftPoint = CGPointMake(rightDriftPoint.x - rightDriftSize.width * 0.5 - 10.0 * scale - driftSize.width * 0.5,
	                                    driftGroupY);
	if (self.handedness == CTRPadTouchHandednessSteerRight)
	{
		leftDriftPoint.x = CGRectGetMinX(safe) + CGRectGetMaxX(safe) - leftDriftPoint.x;
		rightDriftPoint.x = CGRectGetMinX(safe) + CGRectGetMaxX(safe) - rightDriftPoint.x;
	}

#define CTRPAD_NORMALIZED_POINT(point) CGPointMake(((point).x - CGRectGetMinX(safe)) / safeWidth, ((point).y - CGRectGetMinY(safe)) / safeHeight)
	CTRPadTouchStickView *stick = (CTRPadTouchStickView *)[self controlWithIdentifier:@"ctrpad.touch.stick"];
	if (!stick.dynamicOriginActive)
	{
		[self setControl:stick normalizedCenter:CTRPAD_NORMALIZED_POINT(stickPoint) size:stickSize mirror:NO];
		stick.restingCenter = stick.center;
	}
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.cross"] normalizedCenter:CTRPAD_NORMALIZED_POINT(gasPoint) size:gasSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.square"] normalizedCenter:CTRPAD_NORMALIZED_POINT(brakePoint) size:brakeSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.circle"] normalizedCenter:CTRPAD_NORMALIZED_POINT(itemPoint) size:itemSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.triangle"] normalizedCenter:CTRPAD_NORMALIZED_POINT(viewPoint) size:viewSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.l1"] normalizedCenter:CTRPAD_NORMALIZED_POINT(leftDriftPoint) size:driftSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.r1"] normalizedCenter:CTRPAD_NORMALIZED_POINT(rightDriftPoint) size:rightDriftSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.start"] normalizedCenter:CTRPAD_NORMALIZED_POINT(startPoint) size:startSize mirror:NO];
	[self setControl:[self controlWithIdentifier:@"ctrpad.touch.select"] normalizedCenter:CTRPAD_NORMALIZED_POINT(selectPoint) size:selectSize mirror:NO];
#undef CTRPAD_NORMALIZED_POINT

	CGFloat steeringZoneWidth = safeWidth * 0.50;
	CGFloat steeringZoneTop = CGRectGetMinY(safe) + safeHeight * 0.38;
	self.steeringZone.frame = CGRectMake(self.handedness == CTRPadTouchHandednessSteerRight
	                                        ? CGRectGetMaxX(safe) - steeringZoneWidth
	                                        : CGRectGetMinX(safe),
	                                    steeringZoneTop,
	                                    steeringZoneWidth,
	                                    CGRectGetMaxY(safe) - steeringZoneTop);
	self.steeringZone.hidden = !self.touchControlsEnabled || self.layoutEditing;
	self.steeringZone.userInteractionEnabled = self.touchControlsEnabled && !self.layoutEditing;

	for (UIView *control in self.editableControls)
	{
		if ([control isKindOfClass:CTRPadInputButton.class])
		{
			CTRPadInputButton *button = (CTRPadInputButton *)control;
			button.controlOpacity = self.layoutEditing ? 0.90 : self.controlOpacity;
			[button updateAppearance];
			button.layer.cornerRadius = [button.accessibilityIdentifier isEqualToString:@"ctrpad.touch.cross"] ||
			                                    [button.accessibilityIdentifier isEqualToString:@"ctrpad.touch.square"] ||
			                                    [button.accessibilityIdentifier isEqualToString:@"ctrpad.touch.circle"] ||
			                                    [button.accessibilityIdentifier isEqualToString:@"ctrpad.touch.triangle"]
			                                ? MIN(CGRectGetWidth(button.bounds), CGRectGetHeight(button.bounds)) * 0.5
			                                : 14.0;
		}
		else if ([control isKindOfClass:CTRPadTouchStickView.class])
		{
			[(CTRPadTouchStickView *)control applyControlOpacity:self.layoutEditing ? 0.90 : self.controlOpacity];
		}
	}

	UIView *settings = [self controlWithIdentifier:@"ctrpad.touch.settings"];
	CGFloat utilityWidth = phone ? 102.0 : 112.0;
	CGFloat utilityHeight = 44.0;
	settings.bounds = CGRectMake(0.0, 0.0, utilityWidth, utilityHeight);
	settings.center = CGPointMake(CGRectGetMaxX(safe) - utilityWidth * 0.5,
	                            CGRectGetMinY(safe) + utilityHeight * 0.5 + 4.0);
	settings.hidden = self.layoutEditing;

	if (self.layoutEditing)
	{
		CGFloat panelWidth = MIN(CGRectGetWidth(safe), phone ? 680.0 : 720.0);
		CGFloat panelHeight = 54.0;
		self.editorPanel.frame = CGRectMake(CGRectGetMidX(safe) - panelWidth * 0.5, CGRectGetMinY(safe), panelWidth, panelHeight);
		self.editorLabel.frame = CGRectMake(12.0, 4.0, MAX(110.0, panelWidth - 304.0), panelHeight - 8.0);
		CGFloat actionsX = panelWidth - 286.0;
		self.editorSmallerButton.frame = CGRectMake(actionsX, 7.0, 44.0, 40.0);
		self.editorLargerButton.frame = CGRectMake(actionsX + 50.0, 7.0, 44.0, 40.0);
		self.editorResetButton.frame = CGRectMake(actionsX + 102.0, 7.0, 78.0, 40.0);
		self.editorDoneButton.frame = CGRectMake(actionsX + 188.0, 7.0, 88.0, 40.0);
		[self.view bringSubviewToFront:self.editorPanel];
	}
}

- (UIButton *)editorButtonWithTitle:(NSString *)title action:(SEL)action
{
	UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
	[button setTitle:title forState:UIControlStateNormal];
	[button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
	button.titleLabel.font = [UIFont systemFontOfSize:14.0 weight:UIFontWeightBold];
	button.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.16];
	button.layer.cornerRadius = 10.0;
	[button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
	return button;
}

- (void)installLayoutEditor
{
	self.editorPanel = [[UIView alloc] initWithFrame:CGRectZero];
	self.editorPanel.backgroundColor = [UIColor colorWithRed:0.025 green:0.035 blue:0.075 alpha:0.94];
	self.editorPanel.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.30].CGColor;
	self.editorPanel.layer.borderWidth = 1.0;
	self.editorPanel.layer.cornerRadius = 14.0;
	self.editorPanel.hidden = YES;
	self.editorPanel.accessibilityIdentifier = @"ctrpad.touch.editor";

	self.editorLabel = [[UILabel alloc] initWithFrame:CGRectZero];
	self.editorLabel.text = @"Tap or drag a control • − / + resizes";
	self.editorLabel.textColor = UIColor.whiteColor;
	self.editorLabel.font = [UIFont systemFontOfSize:13.0 weight:UIFontWeightSemibold];
	self.editorLabel.numberOfLines = 2;
	[self.editorPanel addSubview:self.editorLabel];
	self.editorSmallerButton = [self editorButtonWithTitle:@"−" action:@selector(shrinkSelectedControl)];
	self.editorSmallerButton.accessibilityLabel = @"Make selected control smaller";
	self.editorSmallerButton.accessibilityIdentifier = @"ctrpad.touch.editor.smaller";
	self.editorLargerButton = [self editorButtonWithTitle:@"+" action:@selector(growSelectedControl)];
	self.editorLargerButton.accessibilityLabel = @"Make selected control larger";
	self.editorLargerButton.accessibilityIdentifier = @"ctrpad.touch.editor.larger";
	self.editorResetButton = [self editorButtonWithTitle:@"Reset" action:@selector(resetCurrentLayout)];
	self.editorResetButton.accessibilityIdentifier = @"ctrpad.touch.editor.reset";
	self.editorDoneButton = [self editorButtonWithTitle:@"Done" action:@selector(endLayoutEditing)];
	self.editorDoneButton.backgroundColor = [UIColor colorWithRed:0.13 green:0.42 blue:0.84 alpha:1.0];
	self.editorDoneButton.accessibilityIdentifier = @"ctrpad.touch.editor.done";
	[self.editorPanel addSubview:self.editorSmallerButton];
	[self.editorPanel addSubview:self.editorLargerButton];
	[self.editorPanel addSubview:self.editorResetButton];
	[self.editorPanel addSubview:self.editorDoneButton];
	[self.view addSubview:self.editorPanel];

	for (UIView *control in self.editableControls)
	{
		UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(moveControl:)];
		pan.enabled = NO;
		pan.cancelsTouchesInView = YES;
		[control addGestureRecognizer:pan];
		[self.editGestures addObject:pan];
		UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(selectControl:)];
		tap.enabled = NO;
		tap.cancelsTouchesInView = YES;
		[control addGestureRecognizer:tap];
		[self.editGestures addObject:tap];
	}
}

- (void)updateEditorSelectionLabel
{
	if (self.selectedControl == nil)
	{
		self.editorLabel.text = @"Tap or drag a control • − / + resizes";
		self.editorSmallerButton.enabled = NO;
		self.editorLargerButton.enabled = NO;
		return;
	}
	CGFloat scale = [self.layoutScales[self.selectedControl.accessibilityIdentifier] doubleValue];
	if (!isfinite(scale) || (scale <= 0.0))
		scale = 1.0;
	self.editorLabel.text = [NSString stringWithFormat:@"%@ • Drag to move • Size %.0f%%",
	                                                     self.selectedControl.accessibilityLabel ?: @"Control", scale * 100.0];
	self.editorSmallerButton.enabled = scale > 0.70;
	self.editorLargerButton.enabled = scale < 1.50;
}

- (void)selectControl:(UIGestureRecognizer *)gesture
{
	if (!self.layoutEditing || ![self.editableControls containsObject:gesture.view])
		return;
	self.selectedControl = gesture.view;
	[self updateEditorSelectionLabel];
}

- (void)adjustSelectedControlScaleBy:(CGFloat)delta
{
	UIView *control = self.selectedControl;
	if (!self.layoutEditing || (control == nil))
		return;
	NSString *identifier = control.accessibilityIdentifier;
	CGFloat scale = [self.layoutScales[identifier] doubleValue];
	if (!isfinite(scale) || (scale <= 0.0))
		scale = 1.0;
	scale = MIN(1.50, MAX(0.70, round((scale + delta) * 10.0) / 10.0));
	self.layoutScales[identifier] = @(scale);
	CGRect safe = [self usableBounds];
	if ((CGRectGetWidth(safe) > 0.0) && (CGRectGetHeight(safe) > 0.0))
	{
		self.layoutCenters[identifier] = @[
			@((control.center.x - CGRectGetMinX(safe)) / CGRectGetWidth(safe)),
			@((control.center.y - CGRectGetMinY(safe)) / CGRectGetHeight(safe)),
		];
	}
	[self updateEditorSelectionLabel];
	[self.view setNeedsLayout];
}

- (void)shrinkSelectedControl
{
	[self adjustSelectedControlScaleBy:-0.10];
}

- (void)growSelectedControl
{
	[self adjustSelectedControlScaleBy:0.10];
}

- (void)moveControl:(UIPanGestureRecognizer *)gesture
{
	UIView *control = gesture.view;
	if (!self.layoutEditing || (control == nil))
	{
		return;
	}
	if (gesture.state == UIGestureRecognizerStateBegan)
	{
		[self selectControl:gesture];
	}
	CGPoint translation = [gesture translationInView:self.view];
	control.center = CGPointMake(control.center.x + translation.x, control.center.y + translation.y);
	[gesture setTranslation:CGPointZero inView:self.view];
	[self clampControlToUsableBounds:control];
	CGRect safe = [self usableBounds];
	if ((CGRectGetWidth(safe) > 0.0) && (CGRectGetHeight(safe) > 0.0))
	{
		self.layoutCenters[control.accessibilityIdentifier] = @[
			@((control.center.x - CGRectGetMinX(safe)) / CGRectGetWidth(safe)),
			@((control.center.y - CGRectGetMinY(safe)) / CGRectGetHeight(safe)),
		];
	}
}

- (void)beginLayoutEditing
{
	if (self.layoutEditing || (self.presentedViewController != nil))
	{
		return;
	}
	[self resetControlState];
	self.layoutEditing = YES;
	self.steeringZone.layoutEditing = YES;
	self.steeringZone.hidden = YES;
	self.steeringZone.userInteractionEnabled = NO;
	for (UIView *control in self.editableControls)
	{
		if ([control isKindOfClass:CTRPadInputButton.class])
			((CTRPadInputButton *)control).layoutEditing = YES;
		else if ([control isKindOfClass:CTRPadTouchStickView.class])
			((CTRPadTouchStickView *)control).layoutEditing = YES;
	}
	for (UIGestureRecognizer *gesture in self.editGestures)
		gesture.enabled = YES;
	self.editorPanel.hidden = NO;
	self.selectedControl = nil;
	[self updateEditorSelectionLabel];
	[self.view setNeedsLayout];
	UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, self.editorLabel);
}

- (void)saveCurrentLayout
{
	if (self.layoutProfile.length == 0)
		return;
	if ((self.layoutCenters.count == 0) && (self.layoutScales.count == 0))
	{
		[NSUserDefaults.standardUserDefaults removeObjectForKey:[self layoutStorageKey]];
		return;
	}
	[NSUserDefaults.standardUserDefaults setObject:@{
		@"centers" : [self.layoutCenters copy],
		@"scales" : [self.layoutScales copy],
	} forKey:[self layoutStorageKey]];
}

- (void)endLayoutEditing
{
	if (!self.layoutEditing)
		return;
	[self resetControlState];
	[self saveCurrentLayout];
	self.layoutEditing = NO;
	self.steeringZone.layoutEditing = NO;
	self.steeringZone.hidden = !self.touchControlsEnabled;
	self.steeringZone.userInteractionEnabled = self.touchControlsEnabled;
	for (UIView *control in self.editableControls)
	{
		if ([control isKindOfClass:CTRPadInputButton.class])
			((CTRPadInputButton *)control).layoutEditing = NO;
		else if ([control isKindOfClass:CTRPadTouchStickView.class])
			((CTRPadTouchStickView *)control).layoutEditing = NO;
	}
	for (UIGestureRecognizer *gesture in self.editGestures)
		gesture.enabled = NO;
	self.selectedControl = nil;
	self.editorPanel.hidden = YES;
	[self.view setNeedsLayout];
	UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, @"Touch layout saved.");
}

- (void)resetCurrentLayout
{
	if (self.layoutProfile.length == 0)
		self.layoutProfile = [self currentLayoutProfile];
	[self.layoutCenters removeAllObjects];
	[self.layoutScales removeAllObjects];
	self.selectedControl = nil;
	[NSUserDefaults.standardUserDefaults removeObjectForKey:[self layoutStorageKey]];
	self.layoutProfile = nil;
	[self updateEditorSelectionLabel];
	[self.view setNeedsLayout];
	UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, @"This touch layout was reset.");
}

- (void)resetAllLayouts
{
	for (NSString *profile in @[
		     @"phone-left-v1", @"phone-right-v1", @"tablet-left-v1", @"tablet-right-v1",
		     @"phone-left-simplified-v1", @"phone-right-simplified-v1",
		     @"tablet-left-simplified-v1", @"tablet-right-simplified-v1"
	     ])
	{
		NSString *key = [NSString stringWithFormat:@"%@.%@", s_touchLayoutKeyPrefix, profile];
		[NSUserDefaults.standardUserDefaults removeObjectForKey:key];
	}
	[self.layoutCenters removeAllObjects];
	[self.layoutScales removeAllObjects];
	self.selectedControl = nil;
	self.layoutProfile = nil;
	[self.view setNeedsLayout];
}

@end

static UIWindow *CTRPadTouch_FindGameWindow(void)
{
	for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
	{
		if (![scene isKindOfClass:UIWindowScene.class] || (scene.activationState == UISceneActivationStateUnattached))
		{
			continue;
		}
		for (UIWindow *window in ((UIWindowScene *)scene).windows)
		{
			if (!window.hidden && (window.alpha > 0.0) && (window.windowLevel <= UIWindowLevelNormal + 0.5) &&
			    (window.rootViewController != nil))
			{
				return window;
			}
		}
	}
	return nil;
}

int NativeIOSTouch_Begin(NativeIOSTouchDiscReselectionCallback discReselectionCallback, void *userdata)
{
	if (!NSThread.isMainThread)
	{
		return 0;
	}
	if (s_touchOverlayController != nil)
	{
		return 1;
	}
	s_discReselectionCallback = discReselectionCallback;
	s_discReselectionUserdata = userdata;

	UIWindow *gameWindow = CTRPadTouch_FindGameWindow();
	if (gameWindow == nil)
	{
		s_discReselectionCallback = NULL;
		s_discReselectionUserdata = NULL;
		return 0;
	}

	CTRPadTouchOverlayViewController *controller = [[CTRPadTouchOverlayViewController alloc] init];
	UIViewController *parent = gameWindow.rootViewController;
	UIView *overlay = controller.view;
	[parent addChildViewController:controller];
	overlay.frame = parent.view.bounds;
	overlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	[parent.view addSubview:overlay];
	[parent.view bringSubviewToFront:overlay];
	[controller didMoveToParentViewController:parent];
	s_touchOverlayController = controller;
	Platform_InputTouchSetEnabled(controller.touchControlsEnabled ? 1 : 0);
	return 1;
}

void NativeIOSTouch_End(void)
{
	if (!NSThread.isMainThread)
	{
		return;
	}
	[s_touchOverlayController resetControlState];
	Platform_InputTouchSetEnabled(0);
	s_discReselectionCallback = NULL;
	s_discReselectionUserdata = NULL;
	[s_touchOverlayController willMoveToParentViewController:nil];
	[s_touchOverlayController.view removeFromSuperview];
	[s_touchOverlayController removeFromParentViewController];
	s_touchOverlayController = nil;
}

int NativeIOSTouch_IsConfiguring(void)
{
    return s_touchOverlayController != nil &&
        (s_touchOverlayController.presentedViewController != nil || s_touchOverlayController.layoutEditing);
}
