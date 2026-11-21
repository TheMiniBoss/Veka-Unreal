// Copyright TheMiniBoss. All Rights Reserved.

#include "Samples/VekaSampleView.h"
#include "UMG/VekaHostWidget.h"

#define LOCTEXT_NAMESPACE "VekaSamples"

UVekaSampleViewModel::UVekaSampleViewModel()
{
	auto Item = [](const FText& Name, int32 Price, const TCHAR* Symbol, bool bNew)
	{
		FVekaSampleItem Out;
		Out.Name = Name;
		Out.Price = Price;
		Out.Symbol = Symbol;
		Out.bNew = bNew;
		return Out;
	};
	Items = {
		Item(LOCTEXT("ItemPotion", "Health potion"), 25, TEXT("drop"), false),
		Item(LOCTEXT("ItemShield", "Oak shield"), 60, TEXT("shield"), true),
		Item(LOCTEXT("ItemLantern", "Lantern"), 140, TEXT("lightbulb"), false),
	};
}

void UVekaSampleLibrary::HandleShopEvent(UVekaHostWidget* Host, FName EventName, const FVekaValue& Payload)
{
	UVekaSampleViewModel* Model = Host ? Cast<UVekaSampleViewModel>(Host->ViewModel) : nullptr;
	if (Model && EventName == TEXT("Buy"))
	{
		Model->Gold = FMath::Max(0, Model->Gold - static_cast<int32>(Payload.AsInt()));
		Model->NotifyChanged();
	}
}

namespace Veka::Samples
{
	FVekaDocument MakeShopItemDocument()
	{
		FVekaDocumentBuilder B;
		B.Param(TEXT("title"), EVekaValueType::Text, FVekaValue::Text(LOCTEXT("ItemDefault", "Item")));
		B.Param(TEXT("price"), EVekaValueType::Int, FVekaValue::Int(10));
		B.Param(TEXT("symbol"), EVekaValueType::String, FVekaValue::String(TEXT("cube")));
		B.Slot(TEXT("badge"));
		B.Event(TEXT("buy"), EVekaValueType::Int);

		auto Root = B.Node(TEXT("HStack")).Arg(TEXT("spacing"), B.Lit(10)).Children({
			B.Node(TEXT("Symbol")).Arg(TEXT("name"), B.ParamRef(TEXT("symbol")))
				.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Title2")) } })
				.Modifier(TEXT("symbolRenderingMode"), { { TEXT("mode"), B.Lit(TEXT("Hierarchical")) } }),
			B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(2)).Arg(TEXT("alignment"), B.Lit(TEXT("Leading"))).Children({
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.ParamRef(TEXT("title")))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Headline")) } }),
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.FormatText(LOCTEXT("ItemPrice", "{0} gold"), { B.ParamRef(TEXT("price")) }))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Footnote")) } })
					.Modifier(TEXT("opacity"), { { TEXT("value"), B.Lit(0.6) } }) }),
			B.SlotContent(TEXT("badge")),
			B.Node(TEXT("Spacer")),
			B.Node(TEXT("Button")).Arg(TEXT("title"), B.LitText(LOCTEXT("Buy", "Buy")))
				.Action(FVekaDocumentBuilder::Emit(TEXT("buy"), B.ParamRef(TEXT("price"))))
				.Modifier(TEXT("controlSize"), { { TEXT("size"), B.Lit(TEXT("Small")) } })
		})
			.Comment(TEXT("One article: the caller gives title, price and symbol, and handles buy."))
			.Modifier(TEXT("padding"), { { TEXT("horizontal"), B.Lit(10) }, { TEXT("vertical"), B.Lit(8) } })
			.Modifier(TEXT("background"), { { TEXT("paint"), B.Lit(TEXT("Fill")) }, { TEXT("radius"), B.Lit(10) } });
		B.SetRoot(Root);
		return B.Release();
	}

	FVekaDocument MakeShopDocument(UVekaView* ShopItem)
	{
		FVekaDocumentBuilder B;
		B.State(TEXT("gift"), FVekaValue::Bool(false));
		B.State(TEXT("lastBought"), FVekaValue::String(FString()));
		B.Event(TEXT("Buy"), EVekaValueType::Int);

		using Op = EVekaExprOp;
		const int32 Item = B.Local(TEXT("item"));

		auto Header = B.Node(TEXT("HStack")).Arg(TEXT("spacing"), B.Lit(10)).Children({
			B.Node(TEXT("Symbol")).Arg(TEXT("name"), B.Lit(TEXT("person.circle.fill")))
				.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Title1")) } })
				.Modifier(TEXT("symbolRenderingMode"), { { TEXT("mode"), B.Lit(TEXT("Hierarchical")) } }),
			B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(2)).Arg(TEXT("alignment"), B.Lit(TEXT("Leading"))).Children({
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.Model(TEXT("PlayerName")))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Headline")) } }),
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.FormatText(LOCTEXT("Gold", "{0} gold"), { B.Model(TEXT("Gold")) }))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Footnote")) } })
					.Modifier(TEXT("opacity"), { { TEXT("value"), B.Lit(0.6) } }) }),
			B.Node(TEXT("Spacer")) });

		auto Article = B.Component(ShopItem)
			.Arg(TEXT("title"), B.Member(Item, TEXT("Name")))
			.Arg(TEXT("price"), B.Member(Item, TEXT("Price")))
			.Arg(TEXT("symbol"), B.Member(Item, TEXT("Symbol")))
			.Children({
				B.If(B.Member(Item, TEXT("bNew")), {
					B.Node(TEXT("Text")).Arg(TEXT("text"), B.LitText(LOCTEXT("New", "NEW")))
						.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Caption2")) } })
						.Modifier(TEXT("fontWeight"), { { TEXT("weight"), B.Lit(TEXT("Bold")) } })
						.Modifier(TEXT("foreground"), { { TEXT("paint"), B.Lit(TEXT("Orange")) } }) }).InSlot(TEXT("badge")) })
			.On(TEXT("buy"), {
				FVekaDocumentBuilder::Set(TEXT("lastBought"), B.Member(Item, TEXT("Name"))),
				FVekaDocumentBuilder::Emit(TEXT("Buy"), B.Local(TEXT("payload"))) })
			.Modifier(TEXT("disabled"), { { TEXT("value"), B.Binary(Op::Greater, B.Member(Item, TEXT("Price")), B.Model(TEXT("Gold"))) } });

		auto Root = B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(12)).Arg(TEXT("alignment"), B.Lit(TEXT("Leading"))).Children({
			Header,
			B.Node(TEXT("ProgressView")).Arg(TEXT("value"), B.Model(TEXT("Health")))
				.Arg(TEXT("label"), B.FormatText(LOCTEXT("Health", "Health {0} %"), { B.Binary(Op::Multiply, B.Model(TEXT("Health")), B.Lit(100)) })),
			B.Node(TEXT("Divider")),
			B.ForEach(B.Model(TEXT("Items")), TEXT("item"), { Article }).IndexVariable(TEXT("index")).Comment(TEXT("One component per article of the model.")),
			B.Node(TEXT("Divider")),
			B.Node(TEXT("Toggle")).Arg(TEXT("title"), B.LitText(LOCTEXT("Gift", "Gift wrap"))).Arg(TEXT("isOn"), B.Var(TEXT("gift"))),
			B.If(B.Var(TEXT("gift")), {
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.LitText(LOCTEXT("Ribbon", "A ribbon will be added.")))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Footnote")) } })
					.Modifier(TEXT("foreground"), { { TEXT("paint"), B.Lit(TEXT("Pink")) } })
					.Modifier(TEXT("transition"), { { TEXT("kind"), B.Lit(TEXT("scale")) } }) }),
			B.If(B.Binary(Op::NotEqual, B.Var(TEXT("lastBought")), B.Lit(TEXT(""))), {
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.FormatText(LOCTEXT("LastBought", "Last bought: {0}"), { B.Var(TEXT("lastBought")) }))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Caption1")) } })
					.Modifier(TEXT("opacity"), { { TEXT("value"), B.Lit(0.7) } }) })
		})
			.Modifier(TEXT("padding"), { { TEXT("value"), B.Lit(20) } })
			.Modifier(TEXT("frame"), { { TEXT("width"), B.Lit(360) } })
			.Modifier(TEXT("background"), { { TEXT("paint"), B.Lit(TEXT("#24242BFF")) }, { TEXT("radius"), B.Lit(18) } })
			.Modifier(TEXT("animation"), { { TEXT("value"), B.Var(TEXT("gift")) } });
		B.SetRoot(Root);
		FVekaDocument Document = B.Release();
		Document.Comments.Add(TEXT("Sample shop: model data, components, slots, events."));
		return Document;
	}

	FVekaDocument MakeStarterDocument()
	{
		FVekaDocumentBuilder B;
		B.State(TEXT("count"), FVekaValue::Int(0));
		auto Root = B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(12)).Children({
			B.Node(TEXT("Text")).Arg(TEXT("text"), B.LitText(LOCTEXT("Hello", "Hello Veka")))
				.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Title2")) } }),
			B.Node(TEXT("Button")).Arg(TEXT("title"), B.FormatText(LOCTEXT("Tapped", "Tapped {0} times"), { B.Var(TEXT("count")) }))
				.Action(FVekaDocumentBuilder::Increment(TEXT("count"), B.Lit(1), true)) })
			.Modifier(TEXT("padding"), { { TEXT("value"), B.Lit(24) } });
		B.SetRoot(Root);
		return B.Release();
	}
}

#undef LOCTEXT_NAMESPACE
