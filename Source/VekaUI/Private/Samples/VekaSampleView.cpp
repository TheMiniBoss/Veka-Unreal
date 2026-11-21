// Copyright TheMiniBoss. All Rights Reserved.

#include "Samples/VekaSampleView.h"

namespace Veka::Samples
{
	FVekaDocument MakeShopDocument()
	{
		FVekaDocumentBuilder B;
		B.State(TEXT("count"), FVekaValue::Int(1));
		B.State(TEXT("gift"), FVekaValue::Bool(false));
		B.Event(TEXT("Buy"));

		using Op = EVekaExprOp;
		const int32 Price = B.Binary(Op::Multiply, B.Var(TEXT("count")), B.Lit(25));
		const int32 TooExpensive = B.Binary(Op::Greater, Price, B.Model(TEXT("Gold")));

		auto Header = B.Node(TEXT("HStack")).Arg(TEXT("spacing"), B.Lit(10)).Children({
			B.Node(TEXT("Symbol")).Arg(TEXT("name"), B.Lit(TEXT("person.circle.fill")))
				.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Title1")) } })
				.Modifier(TEXT("symbolRenderingMode"), { { TEXT("mode"), B.Lit(TEXT("Hierarchical")) } }),
			B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(2)).Arg(TEXT("alignment"), B.Lit(TEXT("Leading"))).Children({
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.Model(TEXT("PlayerName")))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Headline")) } }),
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.Format(TEXT("{0} gold"), { B.Model(TEXT("Gold")) }))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Footnote")) } })
					.Modifier(TEXT("opacity"), { { TEXT("value"), B.Lit(0.6) } }) }),
			B.Node(TEXT("Spacer")) });

		auto Root = B.Node(TEXT("VStack")).Arg(TEXT("spacing"), B.Lit(14)).Arg(TEXT("alignment"), B.Lit(TEXT("Leading"))).Children({
			Header,
			B.Node(TEXT("ProgressView")).Arg(TEXT("value"), B.Model(TEXT("Health")))
				.Arg(TEXT("label"), B.Format(TEXT("Health {0} %"), { B.Binary(Op::Multiply, B.Model(TEXT("Health")), B.Lit(100)) })),
			B.Node(TEXT("Divider")),
			B.Node(TEXT("Stepper")).Arg(TEXT("title"), B.Lit(TEXT("Potions"))).Arg(TEXT("value"), B.Var(TEXT("count")))
				.Arg(TEXT("min"), B.Lit(1)).Arg(TEXT("max"), B.Lit(9)),
			B.Node(TEXT("Toggle")).Arg(TEXT("title"), B.Lit(TEXT("Gift wrap"))).Arg(TEXT("isOn"), B.Var(TEXT("gift"))),
			B.If(B.Var(TEXT("gift")), {
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.Lit(TEXT("A ribbon will be added.")))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Footnote")) } })
					.Modifier(TEXT("foreground"), { { TEXT("paint"), B.Lit(TEXT("Pink")) } })
					.Modifier(TEXT("transition"), { { TEXT("kind"), B.Lit(TEXT("scale")) } }) }),
			B.Node(TEXT("Button"))
				.Arg(TEXT("title"), B.Format(TEXT("Buy {0} for {1} gold"), { B.Var(TEXT("count")), Price }))
				.Arg(TEXT("symbol"), B.Lit(TEXT("cart")))
				.Action(FVekaDocumentBuilder::Emit(TEXT("Buy"), B.Var(TEXT("count"))))
				.Modifier(TEXT("buttonStyle"), { { TEXT("style"), B.Lit(TEXT("BorderedProminent")) } })
				.Modifier(TEXT("disabled"), { { TEXT("value"), TooExpensive } }),
			B.If(TooExpensive, {
				B.Node(TEXT("Text")).Arg(TEXT("text"), B.Lit(TEXT("Not enough gold")))
					.Modifier(TEXT("font"), { { TEXT("style"), B.Lit(TEXT("Caption1")) } })
					.Modifier(TEXT("foreground"), { { TEXT("paint"), B.Lit(TEXT("Red")) } }) })
		})
			.Modifier(TEXT("padding"), { { TEXT("value"), B.Lit(20) } })
			.Modifier(TEXT("frame"), { { TEXT("width"), B.Lit(340) } })
			.Modifier(TEXT("background"), { { TEXT("paint"), B.Lit(TEXT("#24242BFF")) }, { TEXT("radius"), B.Lit(18) } })
			.Modifier(TEXT("animation"), { { TEXT("value"), B.Var(TEXT("gift")) } });
		B.SetRoot(Root);
		return B.Release();
	}
}
