// Copyright TheMiniBoss. All Rights Reserved.

// Plain words for the catalog: a title, an icon, a family and one sentence per component and modifier, a label, a
// sentence and a unit per argument. The Simple mode (palette, outliner, inspector) and the Hub's reference show them.

#include "View/VekaCatalog.h"

#define LOCTEXT_NAMESPACE "VekaCatalogDocs"

namespace
{
	FVekaArgumentDoc Arg(const TCHAR* Name, const FText& Label, const FText& Help, const TCHAR* Unit = TEXT(""))
	{
		FVekaArgumentDoc Out;
		Out.Name = Name;
		Out.Label = Label;
		Out.Help = Help;
		Out.Unit = FText::FromString(Unit);
		return Out;
	}

	FVekaCatalogDoc Doc(const TCHAR* Name, const FText& Title, const TCHAR* Icon, const FText& Category, const FText& Description, TArray<FVekaArgumentDoc> Arguments = {})
	{
		FVekaCatalogDoc Out;
		Out.Name = Name;
		Out.Title = Title;
		Out.Icon = Icon;
		Out.Category = Category;
		Out.Description = Description;
		Out.Arguments = MoveTemp(Arguments);
		return Out;
	}

	// Shared argument words.
	FVekaArgumentDoc Fill() { return Arg(TEXT("fill"), LOCTEXT("Fill", "Fill"), LOCTEXT("FillHelp", "Its color: a theme color (.Accent, .Red...), a #hex color or a paint of the project.")); }
	FVekaArgumentDoc Color(const TCHAR* Name) { return Arg(Name, LOCTEXT("Color", "Color"), LOCTEXT("ColorHelp", "A theme color (.Accent, .Red...), a #hex color or a paint of the project.")); }
	FVekaArgumentDoc Corner() { return Arg(TEXT("cornerRadius"), LOCTEXT("CornerRadius", "Corner radius"), LOCTEXT("CornerRadiusHelp", "How round the corners are (0: square)."), TEXT("px")); }
	FVekaArgumentDoc Radius() { return Arg(TEXT("radius"), LOCTEXT("CornerRadius", "Corner radius"), LOCTEXT("CornerRadiusHelp", "How round the corners are (0: square)."), TEXT("px")); }
	FVekaArgumentDoc Title() { return Arg(TEXT("title"), LOCTEXT("Title", "Title"), LOCTEXT("TitleHelp", "The text it shows (translatable). Use \\(state) to show a value.")); }
	FVekaArgumentDoc SymbolArg() { return Arg(TEXT("symbol"), LOCTEXT("Symbol", "Symbol"), LOCTEXT("SymbolHelp", "An icon of the symbol library, shown before the text.")); }
	FVekaArgumentDoc Tint() { return Arg(TEXT("tint"), LOCTEXT("Tint", "Tint"), LOCTEXT("TintHelp", "A color mixed over the background seen through it.")); }
	FVekaArgumentDoc TintAmount() { return Arg(TEXT("tintAmount"), LOCTEXT("TintAmount", "Tint amount"), LOCTEXT("TintAmountHelp", "0: no tint, 1: only the tint.")); }
	FVekaArgumentDoc Blur() { return Arg(TEXT("blur"), LOCTEXT("Blur", "Blur"), LOCTEXT("BlurHelp", "How blurred the background is."), TEXT("px")); }
	FVekaArgumentDoc Minimum() { return Arg(TEXT("min"), LOCTEXT("Minimum", "Minimum"), LOCTEXT("MinimumHelp", "The smallest value.")); }
	FVekaArgumentDoc Maximum() { return Arg(TEXT("max"), LOCTEXT("Maximum", "Maximum"), LOCTEXT("MaximumHelp", "The largest value.")); }
	TArray<FVekaArgumentDoc> GlassArguments()
	{
		return {
			Corner(), Blur(),
			Arg(TEXT("refraction"), LOCTEXT("Refraction", "Refraction"), LOCTEXT("RefractionHelp", "How much the edge bends what is behind."), TEXT("px")),
			Arg(TEXT("bezel"), LOCTEXT("Bezel", "Edge width"), LOCTEXT("BezelHelp", "Width of the curved edge where the light bends."), TEXT("px")),
			Arg(TEXT("aberration"), LOCTEXT("Aberration", "Rainbow"), LOCTEXT("AberrationHelp", "Color split at the edge, 0 to 1.")),
			Arg(TEXT("highlight"), LOCTEXT("Highlight", "Shine"), LOCTEXT("HighlightHelp", "Light on the edge, 0 to 1 (it follows the pointer).")),
			Tint(), TintAmount() };
	}

	const TArray<FVekaCatalogDoc>& ComponentDocs()
	{
		const FText Layout = LOCTEXT("Layout", "Layout");
		const FText Content = LOCTEXT("Content", "Content");
		const FText Controls = LOCTEXT("Controls", "Controls");
		const FText Shapes = LOCTEXT("Shapes", "Shapes");
		static const TArray<FVekaCatalogDoc> Docs = {
			Doc(TEXT("VStack"), LOCTEXT("VStack", "Vertical stack"), TEXT("line.3.horizontal"), Layout, LOCTEXT("VStackHelp", "Puts its views one under the other."), {
				Arg(TEXT("spacing"), LOCTEXT("Spacing", "Spacing"), LOCTEXT("SpacingHelp", "Space between the views."), TEXT("px")),
				Arg(TEXT("alignment"), LOCTEXT("Alignment", "Alignment"), LOCTEXT("VAlignmentHelp", "How narrower views line up: left, center or right.")) }),
			Doc(TEXT("HStack"), LOCTEXT("HStack", "Horizontal stack"), TEXT("pause"), Layout, LOCTEXT("HStackHelp", "Puts its views side by side."), {
				Arg(TEXT("spacing"), LOCTEXT("Spacing", "Spacing"), LOCTEXT("SpacingHelp", "Space between the views."), TEXT("px")),
				Arg(TEXT("alignment"), LOCTEXT("Alignment", "Alignment"), LOCTEXT("HAlignmentHelp", "How shorter views line up: top, center or bottom.")) }),
			Doc(TEXT("ZStack"), LOCTEXT("ZStack", "Layers"), TEXT("rectangle.stack"), Layout, LOCTEXT("ZStackHelp", "Puts its views on top of each other; the last one is in front.")),
			Doc(TEXT("Spacer"), LOCTEXT("Spacer", "Space"), TEXT("arrow.left.right"), Layout, LOCTEXT("SpacerHelp", "Takes all the free space of its stack: pushes the other views apart."), {
				Arg(TEXT("min"), LOCTEXT("MinSpace", "Minimum"), LOCTEXT("MinSpaceHelp", "The smallest space it keeps."), TEXT("px")) }),
			Doc(TEXT("Divider"), LOCTEXT("Divider", "Divider"), TEXT("minus"), Layout, LOCTEXT("DividerHelp", "A thin line between views.")),
			Doc(TEXT("Text"), LOCTEXT("Text", "Text"), TEXT("textformat.size"), Content, LOCTEXT("TextHelp", "Shows text (translatable); \\(state) shows a value inside."), {
				Arg(TEXT("text"), LOCTEXT("TextArg", "Text"), LOCTEXT("TextArgHelp", "What it says. Write \\(count) to show the value of a state.")) }),
			Doc(TEXT("Symbol"), LOCTEXT("SymbolTitle", "Symbol"), TEXT("star"), Content, LOCTEXT("SymbolTitleHelp", "An icon of the symbol library; it follows the text size and color."), {
				Arg(TEXT("name"), LOCTEXT("SymbolName", "Symbol"), LOCTEXT("SymbolNameHelp", "Which icon.")),
				Arg(TEXT("value"), LOCTEXT("VariableValue", "Fill level"), LOCTEXT("VariableValueHelp", "For icons that fill gradually (Wi-Fi, volume), 0 to 1.")) }),
			Doc(TEXT("Label"), LOCTEXT("LabelTitle", "Label"), TEXT("tag"), Content, LOCTEXT("LabelTitleHelp", "A symbol followed by a text."), { Title(), SymbolArg() }),
			Doc(TEXT("Button"), LOCTEXT("Button", "Button"), TEXT("target"), Controls, LOCTEXT("ButtonHelp", "Runs its actions when clicked."), {
				Title(), SymbolArg(),
				Arg(TEXT("role"), LOCTEXT("Role", "Role"), LOCTEXT("RoleHelp", "Destructive turns it red; Cancel marks the way out.")) }),
			Doc(TEXT("Toggle"), LOCTEXT("Toggle", "Toggle"), TEXT("power"), Controls, LOCTEXT("ToggleHelp", "An on / off switch that changes a Bool state."), {
				Title(), Arg(TEXT("isOn"), LOCTEXT("IsOn", "State"), LOCTEXT("IsOnHelp", "The Bool state it turns on and off.")) }),
			Doc(TEXT("Slider"), LOCTEXT("Slider", "Slider"), TEXT("slider.horizontal.3"), Controls, LOCTEXT("SliderHelp", "Changes a number state by dragging."), {
				Arg(TEXT("value"), LOCTEXT("SliderValue", "State"), LOCTEXT("SliderValueHelp", "The number state it changes.")), Minimum(), Maximum(),
				Arg(TEXT("step"), LOCTEXT("Step", "Step"), LOCTEXT("StepHelp", "Moves by this much (0: any value).")) }),
			Doc(TEXT("Stepper"), LOCTEXT("Stepper", "Stepper"), TEXT("plus"), Controls, LOCTEXT("StepperHelp", "- and + buttons around a number state."), {
				Title(), Arg(TEXT("value"), LOCTEXT("StepperValue", "State"), LOCTEXT("StepperValueHelp", "The number state it changes.")), Minimum(), Maximum(),
				Arg(TEXT("step"), LOCTEXT("Step", "Step"), LOCTEXT("StepperStepHelp", "Added or removed by each click.")),
				Arg(TEXT("decimals"), LOCTEXT("Decimals", "Decimals"), LOCTEXT("DecimalsHelp", "Digits shown after the point.")) }),
			Doc(TEXT("TextField"), LOCTEXT("TextField", "Text field"), TEXT("pencil"), Controls, LOCTEXT("TextFieldHelp", "Lets the player type a text into a String state."), {
				Arg(TEXT("placeholder"), LOCTEXT("Placeholder", "Placeholder"), LOCTEXT("PlaceholderHelp", "Shown in gray while it is empty.")),
				Arg(TEXT("text"), LOCTEXT("TextFieldState", "State"), LOCTEXT("TextFieldStateHelp", "The String state it writes.")) }),
			Doc(TEXT("ProgressView"), LOCTEXT("ProgressView", "Progress bar"), TEXT("loading"), Controls, LOCTEXT("ProgressViewHelp", "Shows how far something has got."), {
				Arg(TEXT("value"), LOCTEXT("Progress", "Progress"), LOCTEXT("ProgressHelp", "0: empty, 1: full. A state or a formula moves it.")),
				Arg(TEXT("label"), LOCTEXT("ProgressLabel", "Label"), LOCTEXT("ProgressLabelHelp", "Text shown with the bar.")) }),
			Doc(TEXT("Gauge"), LOCTEXT("Gauge", "Gauge"), TEXT("gauge"), Controls, LOCTEXT("GaugeHelp", "Shows a value between a minimum and a maximum."), {
				Arg(TEXT("value"), LOCTEXT("GaugeValue", "Value"), LOCTEXT("GaugeValueHelp", "What it shows.")), Minimum(), Maximum(),
				Arg(TEXT("label"), LOCTEXT("GaugeLabel", "Label"), LOCTEXT("GaugeLabelHelp", "Text shown with the gauge.")) }),
			Doc(TEXT("Rectangle"), LOCTEXT("Rectangle", "Rectangle"), TEXT("square"), Shapes, LOCTEXT("RectangleHelp", "A filled rectangle; it takes the space it is given."), { Fill() }),
			Doc(TEXT("RoundedRectangle"), LOCTEXT("RoundedRectangle", "Rounded rectangle"), TEXT("square"), Shapes, LOCTEXT("RoundedRectangleHelp", "A rectangle with rounded corners."), { Radius(), Fill() }),
			Doc(TEXT("Capsule"), LOCTEXT("Capsule", "Capsule"), TEXT("capsule"), Shapes, LOCTEXT("CapsuleHelp", "A pill: the short sides are fully round."), { Fill() }),
			Doc(TEXT("Circle"), LOCTEXT("Circle", "Circle"), TEXT("circle"), Shapes, LOCTEXT("CircleHelp", "A circle as big as its frame allows."), { Fill() }),
			Doc(TEXT("Ellipse"), LOCTEXT("Ellipse", "Ellipse"), TEXT("circle"), Shapes, LOCTEXT("EllipseHelp", "An oval that fills its frame."), { Fill() }),
			Doc(TEXT("Canvas"), LOCTEXT("Canvas", "Canvas"), TEXT("paintbrush"), LOCTEXT("Drawing", "Drawing"),
				LOCTEXT("CanvasHelp", "Draws dots, lines and glows with formulas, thousands per frame (particles, charts...). Inside: width, height and time."), {
				Arg(TEXT("blend"), LOCTEXT("CanvasBlend", "Blend"), LOCTEXT("CanvasBlendHelp", "Add: overlapping marks add their light (fireworks, sparks).")) }),
			Doc(TEXT("Dot"), LOCTEXT("Dot", "Dot"), TEXT("circle"), LOCTEXT("Drawing", "Drawing"), LOCTEXT("DotHelp", "In a Canvas: a filled circle."), {
				Arg(TEXT("x"), LOCTEXT("DotX", "X"), LOCTEXT("DotXHelp", "From the left of the canvas."), TEXT("px")),
				Arg(TEXT("y"), LOCTEXT("DotY", "Y"), LOCTEXT("DotYHelp", "From the top of the canvas."), TEXT("px")),
				Arg(TEXT("radius"), LOCTEXT("DotRadius", "Radius"), LOCTEXT("DotRadiusHelp", "Half its size."), TEXT("px")),
				Color(TEXT("color")), Arg(TEXT("opacity"), LOCTEXT("DotOpacity", "Opacity"), LOCTEXT("DotOpacityHelp", "0 to 1.")) }),
			Doc(TEXT("Line"), LOCTEXT("LineMark", "Line"), TEXT("pencil"), LOCTEXT("Drawing", "Drawing"), LOCTEXT("LineMarkHelp", "In a Canvas: a straight line, its opacity can fade along it."), {
				Arg(TEXT("x1"), LOCTEXT("X1", "Start X"), LOCTEXT("X1Help", "Where it starts."), TEXT("px")),
				Arg(TEXT("y1"), LOCTEXT("Y1", "Start Y"), LOCTEXT("Y1Help", "Where it starts."), TEXT("px")),
				Arg(TEXT("x2"), LOCTEXT("X2", "End X"), LOCTEXT("X2Help", "Where it ends."), TEXT("px")),
				Arg(TEXT("y2"), LOCTEXT("Y2", "End Y"), LOCTEXT("Y2Help", "Where it ends."), TEXT("px")),
				Arg(TEXT("width"), LOCTEXT("LineWidthMark", "Thickness"), LOCTEXT("LineWidthMarkHelp", "Thickness of the line."), TEXT("px")),
				Color(TEXT("color")),
				Arg(TEXT("fromOpacity"), LOCTEXT("FromOpacity", "Start opacity"), LOCTEXT("FromOpacityHelp", "Opacity at the start (a trail: 0).")),
				Arg(TEXT("opacity"), LOCTEXT("LineOpacity", "End opacity"), LOCTEXT("LineOpacityHelp", "Opacity at the end, 0 to 1.")) }),
			Doc(TEXT("Glow"), LOCTEXT("Glow", "Glow"), TEXT("sparkle"), LOCTEXT("Drawing", "Drawing"), LOCTEXT("GlowHelp", "In a Canvas: a soft light, bright in the middle, fading to nothing."), {
				Arg(TEXT("x"), LOCTEXT("GlowX", "X"), LOCTEXT("DotXHelp", "From the left of the canvas."), TEXT("px")),
				Arg(TEXT("y"), LOCTEXT("GlowY", "Y"), LOCTEXT("DotYHelp", "From the top of the canvas."), TEXT("px")),
				Arg(TEXT("radius"), LOCTEXT("GlowRadius", "Radius"), LOCTEXT("GlowRadiusHelp", "Where it fades out."), TEXT("px")),
				Color(TEXT("color")), Arg(TEXT("opacity"), LOCTEXT("GlowOpacity", "Opacity"), LOCTEXT("GlowOpacityHelp", "In the middle, 0 to 1.")) }),
		};
		return Docs;
	}

	const TArray<FVekaCatalogDoc>& ModifierDocs()
	{
		const FText Layout = LOCTEXT("Layout", "Layout");
		const FText Style = LOCTEXT("Style", "Style");
		const FText TextFamily = LOCTEXT("TextFamily", "Text");
		const FText Controls = LOCTEXT("Controls", "Controls");
		const FText Effects = LOCTEXT("Effects", "Effects");
		const FText Animation = LOCTEXT("Animation", "Animation");
		const FText Interaction = LOCTEXT("Interaction", "Interaction");
		static const TArray<FVekaCatalogDoc> Docs = {
			Doc(TEXT("padding"), LOCTEXT("Padding", "Padding"), TEXT("arrow.compress"), Layout, LOCTEXT("PaddingHelp", "Empty space around the view, inside its frame."), {
				Arg(TEXT("value"), LOCTEXT("AllSides", "All sides"), LOCTEXT("AllSidesHelp", "The same space on every side."), TEXT("px")),
				Arg(TEXT("horizontal"), LOCTEXT("Horizontal", "Left & right"), LOCTEXT("HorizontalHelp", "Space on the left and on the right."), TEXT("px")),
				Arg(TEXT("vertical"), LOCTEXT("Vertical", "Top & bottom"), LOCTEXT("VerticalHelp", "Space above and below."), TEXT("px")) }),
			Doc(TEXT("frame"), LOCTEXT("Frame", "Size"), TEXT("arrow.expand"), Layout, LOCTEXT("FrameHelp", "Gives the view a width and / or a height of its own."), {
				Arg(TEXT("width"), LOCTEXT("Width", "Width"), LOCTEXT("WidthHelp", "Empty: as wide as its content."), TEXT("px")),
				Arg(TEXT("height"), LOCTEXT("Height", "Height"), LOCTEXT("HeightHelp", "Empty: as tall as its content."), TEXT("px")),
				Arg(TEXT("fillWidth"), LOCTEXT("FillWidth", "Fill the width"), LOCTEXT("FillWidthHelp", "Take all the width available.")),
				Arg(TEXT("alignment"), LOCTEXT("FrameAlignment", "Alignment"), LOCTEXT("FrameAlignmentHelp", "Where the content sits when the frame is larger.")) }),
			Doc(TEXT("offset"), LOCTEXT("Offset", "Offset"), TEXT("arrow.move"), Layout, LOCTEXT("OffsetHelp", "Moves the drawing; the layout keeps its place."), {
				Arg(TEXT("x"), LOCTEXT("X", "X"), LOCTEXT("XHelp", "To the right if positive."), TEXT("px")),
				Arg(TEXT("y"), LOCTEXT("Y", "Y"), LOCTEXT("YHelp", "Down if positive."), TEXT("px")) }),
			Doc(TEXT("scale"), LOCTEXT("Scale", "Scale"), TEXT("magnifyingglass"), Layout, LOCTEXT("ScaleHelp", "Draws the view bigger or smaller around its center (1: normal)."), {
				Arg(TEXT("value"), LOCTEXT("ScaleValue", "Scale"), LOCTEXT("ScaleValueHelp", "2: twice as big, 0.5: half."), TEXT("×")) }),
			Doc(TEXT("rotation"), LOCTEXT("Rotation", "Rotation"), TEXT("arrow.clockwise"), Layout, LOCTEXT("RotationHelp", "Turns the view around its center."), {
				Arg(TEXT("degrees"), LOCTEXT("Angle", "Angle"), LOCTEXT("AngleHelp", "In degrees, clockwise."), TEXT("°")) }),
			Doc(TEXT("background"), LOCTEXT("Background", "Background"), TEXT("paintbrush"), Style, LOCTEXT("BackgroundHelp", "Paints behind the view."), { Color(TEXT("paint")), Radius() }),
			Doc(TEXT("foreground"), LOCTEXT("Foreground", "Content color"), TEXT("drop"), Style, LOCTEXT("ForegroundHelp", "Color of the texts, symbols and lines inside."), { Color(TEXT("paint")) }),
			Doc(TEXT("border"), LOCTEXT("Border", "Border"), TEXT("square"), Style, LOCTEXT("BorderHelp", "A line around the view."), {
				Color(TEXT("paint")), Arg(TEXT("width"), LOCTEXT("LineWidth", "Thickness"), LOCTEXT("LineWidthHelp", "Thickness of the line."), TEXT("px")), Radius() }),
			Doc(TEXT("opacity"), LOCTEXT("Opacity", "Opacity"), TEXT("eye"), Style, LOCTEXT("OpacityHelp", "How visible the view is: 1 fully, 0 invisible."), {
				Arg(TEXT("value"), LOCTEXT("OpacityValue", "Opacity"), LOCTEXT("OpacityValueHelp", "0 to 1.")) }),
			Doc(TEXT("clip"), LOCTEXT("Clip", "Clip"), TEXT("crop"), Style, LOCTEXT("ClipHelp", "Hides whatever sticks out of the view's frame.")),
			Doc(TEXT("style"), LOCTEXT("SharedStyle", "Shared style"), TEXT("paintpalette"), Style, LOCTEXT("SharedStyleHelp", "Applies a style of a component library (.Card...)."), {
				Arg(TEXT("name"), LOCTEXT("StyleName", "Style"), LOCTEXT("StyleNameHelp", "Which style.")) }),
			Doc(TEXT("symbolRenderingMode"), LOCTEXT("SymbolColors", "Symbol colors"), TEXT("star"), Style, LOCTEXT("SymbolColorsHelp", "How symbols inside are colored: one color, shades, palette or their own colors."), {
				Arg(TEXT("mode"), LOCTEXT("Mode", "Mode"), LOCTEXT("SymbolModeHelp", "Monochrome, Hierarchical, Palette or Multicolor.")) }),
			Doc(TEXT("font"), LOCTEXT("Font", "Text style"), TEXT("textformat.size"), TextFamily, LOCTEXT("FontHelp", "Size and weight of the theme's text styles: Title, Body, Caption..."), {
				Arg(TEXT("style"), LOCTEXT("TextStyle", "Style"), LOCTEXT("TextStyleHelp", "From LargeTitle (biggest) to Caption2 (smallest).")) }),
			Doc(TEXT("fontWeight"), LOCTEXT("FontWeight", "Weight"), TEXT("textformat.size"), TextFamily, LOCTEXT("FontWeightHelp", "How thick the letters are, from Hairline to Black."), {
				Arg(TEXT("weight"), LOCTEXT("Weight", "Weight"), LOCTEXT("WeightHelp", "Regular for text, SemiBold or Bold for titles.")) }),
			Doc(TEXT("fontSize"), LOCTEXT("FontSize", "Font size"), TEXT("textformat.size"), TextFamily, LOCTEXT("FontSizeHelp", "A size in points instead of a text style."), {
				Arg(TEXT("size"), LOCTEXT("Size", "Size"), LOCTEXT("SizeHelp", "Height of the letters."), TEXT("pt")) }),
			Doc(TEXT("buttonStyle"), LOCTEXT("ButtonStyle", "Button style"), TEXT("target"), Controls, LOCTEXT("ButtonStyleHelp", "How the buttons inside look."), {
				Arg(TEXT("style"), LOCTEXT("ButtonStyleArg", "Style"), LOCTEXT("ButtonStyleArgHelp", "Plain, Borderless, Bordered or BorderedProminent (filled).")) }),
			Doc(TEXT("toggleStyle"), LOCTEXT("ToggleStyle", "Toggle style"), TEXT("power"), Controls, LOCTEXT("ToggleStyleHelp", "How the toggles inside look."), {
				Arg(TEXT("style"), LOCTEXT("ToggleStyleArg", "Style"), LOCTEXT("ToggleStyleArgHelp", "Switch, Button or Checkbox.")) }),
			Doc(TEXT("controlSize"), LOCTEXT("ControlSize", "Control size"), TEXT("slider.horizontal.3"), Controls, LOCTEXT("ControlSizeHelp", "Size of the controls inside."), {
				Arg(TEXT("size"), LOCTEXT("ControlSizeArg", "Size"), LOCTEXT("ControlSizeArgHelp", "Mini, Small, Regular or Large.")) }),
			Doc(TEXT("backdropBlur"), LOCTEXT("BackdropBlur", "Background blur"), TEXT("cloud"), Effects, LOCTEXT("BackdropBlurHelp", "Blurs what is behind the view (the 3D scene, the UI)."), {
				Arg(TEXT("radius"), LOCTEXT("BlurRadius", "Blur"), LOCTEXT("BlurHelp", "How blurred the background is."), TEXT("px")), Corner(), Tint(), TintAmount(),
				Arg(TEXT("saturation"), LOCTEXT("Saturation", "Saturation"), LOCTEXT("SaturationHelp", "1: as is, more: more vivid colors.")) }),
			Doc(TEXT("frostedGlass"), LOCTEXT("FrostedGlass", "Frosted glass"), TEXT("snowflake"), Effects, LOCTEXT("FrostedGlassHelp", "A blurred, tinted, grainy pane behind the view."), {
				Blur(), Corner(), Tint(), Arg(TEXT("noise"), LOCTEXT("Noise", "Grain"), LOCTEXT("NoiseHelp", "0: smooth.")) }),
			Doc(TEXT("glass"), LOCTEXT("Glass", "Liquid glass"), TEXT("sparkle"), Effects, LOCTEXT("GlassHelp", "Glass that bends the light at its edges."), GlassArguments()),
			Doc(TEXT("glassGroup"), LOCTEXT("GlassGroup", "Glass group"), TEXT("bubble.2"), Effects, LOCTEXT("GlassGroupHelp", "Liquid glass views inside melt together when they get close."), [] {
				TArray<FVekaArgumentDoc> Out = { Arg(TEXT("smoothness"), LOCTEXT("Smoothness", "Merge distance"), LOCTEXT("SmoothnessHelp", "How close two glass views get before they melt."), TEXT("px")) };
				Out.Append(GlassArguments());
				return Out;
			}()),
			Doc(TEXT("blendMode"), LOCTEXT("BlendMode", "Blend mode"), TEXT("circle"), Effects, LOCTEXT("BlendModeHelp", "Mixes the view with what is behind it: multiply darkens, screen lightens..."), {
				Arg(TEXT("mode"), LOCTEXT("BlendModeArg", "Mode"), LOCTEXT("BlendModeArgHelp", "Multiply, Screen, Overlay, Darken, Lighten...")) }),
			Doc(TEXT("liquidDistortion"), LOCTEXT("LiquidDistortion", "Liquid distortion"), TEXT("waveform"), Effects, LOCTEXT("LiquidDistortionHelp", "Ripples what is behind the view, like water."), {
				Arg(TEXT("strength"), LOCTEXT("Strength", "Strength"), LOCTEXT("StrengthHelp", "How far the background moves."), TEXT("px")),
				Arg(TEXT("scale"), LOCTEXT("WaveSize", "Wave size"), LOCTEXT("WaveSizeHelp", "Size of the ripples."), TEXT("px")),
				Arg(TEXT("speed"), LOCTEXT("Speed", "Speed"), LOCTEXT("SpeedHelp", "How fast the ripples move.")), Corner() }),
			Doc(TEXT("animation"), LOCTEXT("AnimationMod", "Animate changes"), TEXT("sparkles"), Animation, LOCTEXT("AnimationModHelp", "When the watched value changes, the view moves smoothly to its new look."), {
				Arg(TEXT("value"), LOCTEXT("Watched", "When it changes"), LOCTEXT("WatchedHelp", "The state to watch.")) }),
			Doc(TEXT("transition"), LOCTEXT("Transition", "Appear / disappear"), TEXT("wand.and.stars"), Animation, LOCTEXT("TransitionHelp", "How the view comes in and goes out (inside an if)."), {
				Arg(TEXT("kind"), LOCTEXT("TransitionKind", "Effect"), LOCTEXT("TransitionKindHelp", "opacity fades, scale grows, slide comes from the side, move drops from the top, blur comes into focus.")) }),
			Doc(TEXT("onTap"), LOCTEXT("OnTap", "On click"), TEXT("hand.thumbsup"), Interaction, LOCTEXT("OnTapHelp", "Runs actions when the view is clicked.")),
			Doc(TEXT("on"), LOCTEXT("On", "On event"), TEXT("bolt"), Interaction, LOCTEXT("OnHelp", "Runs actions when the component sends an event."), {
				Arg(TEXT("event"), LOCTEXT("Event", "Event"), LOCTEXT("EventHelp", "Which event of the component.")) }),
			Doc(TEXT("disabled"), LOCTEXT("Disabled", "Disabled"), TEXT("nosign"), Interaction, LOCTEXT("DisabledHelp", "Grays the view out; it ignores clicks."), {
				Arg(TEXT("value"), LOCTEXT("DisabledValue", "Disabled"), LOCTEXT("DisabledValueHelp", "On: disabled. A state or a condition can drive it.")) }),
		};
		return Docs;
	}
}

namespace Veka::Catalog
{
	const FVekaCatalogDoc* FindDoc(FName Name, bool bModifier)
	{
		const TArray<FVekaCatalogDoc>& Docs = bModifier ? ModifierDocs() : ComponentDocs();
		return Docs.FindByPredicate([Name](const FVekaCatalogDoc& Doc) { return Doc.Name == Name; });
	}

	FText ArgumentLabel(FName Owner, bool bModifier, FName Argument)
	{
		if (const FVekaCatalogDoc* Doc = FindDoc(Owner, bModifier))
		{
			if (const FVekaArgumentDoc* ArgumentDoc = Doc->FindArgument(Argument))
			{
				return ArgumentDoc->Label;
			}
		}
		// isOn -> Is on, cornerRadius -> Corner radius.
		const FString Raw = Argument.ToString();
		FString Out;
		for (int32 Index = 0; Index < Raw.Len(); ++Index)
		{
			const TCHAR C = Raw[Index];
			if (Index > 0 && FChar::IsUpper(C))
			{
				Out.AppendChar(TEXT(' '));
				Out.AppendChar(FChar::ToLower(C));
			}
			else
			{
				Out.AppendChar(Index == 0 ? FChar::ToUpper(C) : C);
			}
		}
		return FText::FromString(Out);
	}
}

#undef LOCTEXT_NAMESPACE
