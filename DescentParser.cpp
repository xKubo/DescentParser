// DescentParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <variant>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <cassert>
#include <functional>
#include <optional>
#define _USE_MATH_DEFINES
#include <math.h>
#include <map>
#include <sstream>  
#include <fstream>
#include <algorithm>

using namespace std;
using namespace std::string_literals;



[[noreturn]] void ThrowError(string Msg)
{
	throw std::runtime_error("Error  : " + Msg);
}


struct ETokenError : std::runtime_error
{
	ETokenError(string Msg, string_view Loc) : std::runtime_error(Msg), m_Location(Loc)
	{

	}

	string_view Location() const
	{
		return m_Location;
	}

	string_view m_Location;
};

[[noreturn]] void ThrowError(string Msg, string_view s)
{
	throw ETokenError("Error : " + Msg, s);
}

using Result = double;
using Results = vector<Result>;

using TFunction = std::function<Result(const Results&)>;

template <typename T>
struct get_arity : get_arity<decltype(&T::operator())> {};
template <typename R, typename... Args>
struct get_arity<R(*)(Args...)> : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct get_arity<R(C::*)(Args...)> : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct get_arity<R(C::*)(Args...) const> : std::integral_constant<unsigned, sizeof...(Args)> {};


struct CFunctions
{
	CFunctions() = default;

	template <typename F>
	void Add(string Name, F f)
	{
		auto l = [f](const Results &rs)
		{
			constexpr int Arity = get_arity<F>::value;
			if (rs.size() != Arity)
				ThrowError("Param size mismatch.");
			using ISeq = std::make_index_sequence<Arity>;
			return CallFn(rs, f, ISeq{});
		};
		m_Fns[Name] = l;
	}

	Result Apply(string Name, const Results &Args) const
	{
		auto it = m_Fns.find(Name);
		if (it == m_Fns.cend())
			ThrowError("Invalid Function : " + Name);
		return it->second(Args);
	}
private:
	template <typename F, std::size_t... I>
	static inline Result CallFn(const Results& rs, F f, std::index_sequence<I...>)
	{
		return f(rs[I]...);
	}

	map<string, TFunction> m_Fns;
};

auto Power = [](Result b, Result e) {return pow(b, e); };

const CFunctions Fns = [] {
	CFunctions f;
	f.Add("abs", [](Result v) {return abs(v); });
	f.Add("hypot", [](Result a, Result b) { return hypot(a, b); });
	f.Add("sin", [](Result x) {return sin(x); });
	f.Add("cos", [](Result x) {return cos(x); });
	f.Add("tan", [](Result x) {return tan(x); });
	f.Add("cotg", [](Result x) {return 1/tan(x); });
	f.Add("pow", Power);
	f.Add("log", [](Result x) {return log(x); });
	f.Add("sqrt", [](Result x) {return sqrt(x); });

	return f;
}();

const map<string, Result> Constants = {
	{"pi", M_PI},
	{"e", M_E},
};

Result GetConstant(const string& Name)
{
	auto it = Constants.find(Name);
	if (it == Constants.end())
		ThrowError("Invalid constant: " + Name);
	return it->second;
}

struct TokenBase
{
	string_view Location;
};


struct CSymbol : TokenBase
{
	using CFn = std::function<Result(Result, Result)>;
	CSymbol(int p, string s, CSymbol::CFn f) : m_Precedence(p), SymbolSign(s), m_Fn(f) {}
	
	int m_Precedence;
	string SymbolSign;
	CFn m_Fn;
};

template <typename TElement>
struct CSymBase
{
	void Sort()
	{
		std::sort(m_Elems.begin(), m_Elems.end(), [](const auto& e1, const auto& e2) {return e1.SymbolSign.size() > e2.SymbolSign.size(); });
	}

	optional<TElement> Parse(string_view& sv) const
	{
		for (const auto& s : m_Elems)
		{
			if (sv.substr(0, s.SymbolSign.size()) == s.SymbolSign)
			{
				sv = sv.substr(s.SymbolSign.size());
				return s;
			}
		}

		return {};
	}

	int Size() const
	{
		return m_Elems.size();
	}

protected:
	vector<TElement> m_Elems;
};

struct CSymbols : CSymBase<CSymbol>
{
	
	void Add(int Precedence, string Symbol, CSymbol::CFn f)
	{
		m_Elems.push_back({ Precedence, Symbol, f });
	}

	
};


struct Assignment : TokenBase
{
	template <typename F>
	Assignment(string Symbol, F f) : SymbolSign(Symbol)
	{
		Apply = f;
	}
	std::function<Result(Result, Result)> Apply = [](Result lhs, Result rhs) {return rhs; };
	string SymbolSign;
};

struct CAssignments : CSymBase<Assignment>
{
	void Add(string Symbol, CSymbol::CFn f)
	{
		m_Elems.push_back({ Symbol, f });
	}
};


const CSymbols Syms = [] {
	CSymbols s;


	s.Add(0, "<", [](Result a, Result b) {return a < b; });
	s.Add(0, ">", [](Result a, Result b) {return a > b; });
	s.Add(0, "<=", [](Result a, Result b) {return a <= b; });
	s.Add(0, ">=", [](Result a, Result b) {return a >= b; });
	s.Add(0, "==", [](Result a, Result b) {return a == b; });
	s.Add(0, "!=", [](Result a, Result b) {return a != b; });

	s.Add(1, "+", [](Result a, Result b) {return a + b; });
	s.Add(1, "-", [](Result a, Result b) {return a - b; });
	s.Add(1, "||", [](Result a, Result b) {return a || b; });
	s.Add(1, "|", [](Result a, Result b) {return (int)a | (int)b; });

	s.Add(2, "*", [](Result a, Result b) {return a * b; });
	s.Add(2, "**", Power);
	s.Add(2, "/", [](Result a, Result b) {return (int)a / (int)b; });
	s.Add(2, ":", [](Result a, Result b) {return a / b; });
	s.Add(2, "&", [](Result a, Result b) {return (int)a & (int)b; });
	s.Add(2, "&&", [](Result a, Result b) {return a && b; });

	s.Add(2, "<<", [](Result a, Result b) {return (int)a >> (int)b; });
	s.Add(2, ">>", [](Result a, Result b) {return (int)a >> (int)b; });


	s.Sort();

	return s;
}();

const CAssignments Assignments = []{
	CAssignments a;
	a.Add("=", [](Result l, Result r) {return r; });
	a.Add("+=", [](Result l, Result r) {return l + r; });
	a.Add("-=", [](Result l, Result r) {return l - r; });
	a.Add("*=", [](Result l, Result r) {return l * r; });
	a.Add("**=", [](Result l, Result r) {return pow(l,r); });
	a.Add("/=", [](Result l, Result r) {return (int)l / (int)r; });
	a.Add(":=", [](Result l, Result r) {return l / r; });

	a.Add("|=", [](Result a, Result b) {return (int)a | (int)b; });
	a.Add("&=", [](Result a, Result b) {return (int)a & (int)b; });

	a.Add("<<=", [](Result a, Result b) {return (int)a >> (int)b; });
	a.Add(">>=", [](Result a, Result b) {return (int)a >> (int)b; });

	return a;
}();

struct Num : TokenBase
{
	Num(double Val) : Value(Val) {}
	Num(int x) : Value(static_cast<double>(x)) {}
	double Value;
};



struct LParen : TokenBase {};
struct RParen : TokenBase {};
struct Word : TokenBase { string Value; };
struct End : TokenBase {};
struct Comma : TokenBase {};
struct AtSign : TokenBase {};

using Token = variant<Num, LParen, RParen, End, Word, Comma, CSymbol, AtSign, Assignment>;

template <class... F>
struct overload : F... {
	overload(F... f) : F(f)... {}
};

template <class... F>
auto OverLoad(F... f) {
	return overload<F...>(f...);
}

TokenBase &ToBase(Token& t)
{
	auto l = [](auto& v) -> TokenBase& { return v; } ;
	return std::visit(l, t);
}

void CheckNotEmpty(string_view& s)
{
	if (s.empty())
		throw std::runtime_error("View is empty");
}

void SkipChar(string_view& s)
{
	CheckNotEmpty(s);
	s = s.substr(1);
}

char TakeChar(string_view& s)
{
	CheckNotEmpty(s);
	char r = s[0];
	SkipChar(s);
	return r;
}

char PeekChar(string_view& s)
{
	CheckNotEmpty(s);
	return s[0];
}

optional<char> PeekCharOpt(string_view &s)
{
	if (s.empty())
		return {};
	return s[0];
}

bool IsNum(char c)
{
	return c >= '0' && c <= '9';
}

bool IsNum(optional<char> c)
{
	if (!c)
		return false;
	return IsNum(*c);
}

bool IsLetter(char c)
{
	return c >= 'a' && c <= 'z';
}

bool IsLetter(optional<char> c)
{
	if (!c)
		return false;
	return IsLetter(*c);
}

bool Equals(optional<char> c1, char c2)
{
	if (!c1)
		return false;
	return *c1 == c2;
}

bool GuessChar(string_view& s, char c)
{
	if (!Equals(PeekCharOpt(s), c))
		return false;
	TakeChar(s);
	return true;
}

pair<int, int> ParseInt(string_view &s)
{
	int Res = 0;
	int Count = 0;
	while (IsNum(PeekCharOpt(s)))
	{
		char c = TakeChar(s);
		++Count;
		Res = 10 * Res + c - '0';
	}
	return { Res, Count };
}

int ParseRadix(string_view &s, int Base)
{
	if (s.empty())
		throw std::runtime_error("Invalid Radix number");
	int Res = 0;
	while (true)
	{
		if (s.empty())
			return Res;
		char c = tolower(PeekChar(s));
		int Val = 0;
		if (IsNum(c))
		{
			TakeChar(s);
			Val = c - '0';
		}
		else if (IsLetter(c))
		{
			TakeChar(s);
			Val = c - 'a' + 10;
		}
		else
			return Res;
		if (Val >= Base)
			ThrowError("Invalid Value for specified radix");
		Res = Base * Res + Val;
	}
}

Num ParseNum(string_view& s)
{
	if (GuessChar(s, '0'))
	{
		if (GuessChar(s, 'x'))
			return ParseRadix(s, 16);
		if (GuessChar(s, 'y'))
			return ParseRadix(s, 2);
		//return ParseRadix(s, 8);
	}

	int Res = ParseInt(s).first;
	if (GuessChar(s, '.'))
	{
		auto[Frac, DigitCount] = ParseInt(s);
		return Res + Frac / pow(10, DigitCount);
	}
	else
		return Res;
}

Word ParseWord(string_view& s)
{
	Word w;
	while (IsLetter(PeekCharOpt(s)))
	{
		w.Value.push_back(TakeChar(s));
	}
	return w;
}

map<char, Token> SpecialChars = {
	{ '(', LParen() },
	{ ')', RParen() },
	{ ',', Comma() },	
	{ '@', AtSign()},
};

optional<Token> IsSpecialChar(char c)
{
	auto it = SpecialChars.find(c);
	if (it == SpecialChars.end())
		return {};
	return it->second;
}


Token ReadNextToken(string_view &s)
{
	while (true)
	{
		char c = PeekChar(s);
		if (IsLetter(c))
			return ParseWord(s);
		if (IsNum(c))
			return ParseNum(s);
		
		if (c == ' ')  // ignore whitespace
		{
			TakeChar(s);
			continue;
		}

		if (auto oToken = IsSpecialChar(c); oToken)
		{
			TakeChar(s);
			return *oToken;
		}

		auto oAssign = Assignments.Parse(s);
		if (oAssign)
			return *oAssign;

		auto oSym = Syms.Parse(s);
		if (oSym)
			return *oSym;
		ThrowError("Invalid Symbol");

	}
}


using Tokens = vector<Token>;


Tokens Tokenize(string_view sv)
{
	Tokens ts;
	while (!sv.empty())
	{
		string_view o = sv;
		Token t = ReadNextToken(sv);
		ToBase(t).Location = string_view(o.data(), sv.data() - o.data());
		ts.push_back(t);
	}
	ts.push_back(End{});
	return ts;
}

Token TakeToken(Tokens& t)
{
	assert(!t.empty());
	auto el = t[0];
	t.erase(t.begin());
	return el;
}

const Token &PeekToken(const Tokens& t)
{
	assert(!t.empty());
	return t[0];
}

template <typename T>
optional<T> GuessNextToken(Tokens& ts)
{
	auto t = PeekToken(ts);
	if (HasType<T>(t))
		return TakeTypedToken<T>(ts);
	else
		return {};
}

template <typename TType, typename TVariant>
bool HasType(const TVariant& v)
{
	return std::holds_alternative<TType>(v);
}

template <typename T>
T TakeTypedToken(Tokens &ts)
{
	Token t = TakeToken(ts);
	if (!HasType<T>(t))
		ThrowError("Invalid token type:");
	return get<T>(t);
}

struct CMemory
{
	void Add(const string& VarName, const Result& Value)
	{
		m_Results[VarName] = Value;
	}

	void Clear()
	{
		return m_Results.clear();
	}

	void Erase(const string &VarName)
	{
		m_Results.erase(VarName);
	}

	int Size()
	{
		return m_Results.size();
	}

	Result Get(const string& VarName)
	{
		auto oR = TryGet(VarName);
		if (!oR)
			return {};
		return *oR;
	}

	optional<Result> TryGet(const string &VarName)
	{
		auto it = m_Results.find(VarName);
		if (it == m_Results.end())
			return {};
		return it->second;
	}

	string Dump() const
	{
		ostringstream oss;
		for (const auto& r : m_Results)
		{
			oss << "[" << r.first << "]=[" << r.second << "] ";
		}
		return oss.str();
	}

	Result LastResult;

private:
	map<string, Result> m_Results;
};

struct CContext
{
	Tokens &ts;
	CMemory &m;
};

Result ParseExpression(CContext &c);

Result ParseParenRest(CContext &c)
{
	Result res = ParseExpression(c);
	Token RightPar = TakeToken(c.ts);
	if (!HasType<RParen>(RightPar))
		ThrowError("Missing RParen", ToBase(RightPar).Location);
	return res;
}

Results ParseFunctionParams(CContext &c)
{
	Results rs;
	Token t = TakeToken(c.ts);
	if (!HasType<LParen>(t))
		ThrowError("Params : Missing (");

	Result r = ParseExpression(c);
	rs.push_back(r);
	while (true)
	{
		Token t = TakeToken(c.ts);
		if (HasType<RParen>(t))
			return rs;
		if (!HasType<Comma>(t))
			ThrowError("Missing ) or ,");
		rs.push_back(ParseExpression(c));
	}
	
}

Result ParseNumOrExp(CContext &c)
{
	Token t = TakeToken(c.ts);

	if (AtSign *pAtSign = get_if<AtSign>(&t))
	{
		Token t = PeekToken(c.ts);
		if (HasType<Word>(t))
			return c.m.Get(TakeTypedToken<Word>(c.ts).Value);
		return c.m.LastResult;
	}

	if (CSymbol *pSymbol = get_if<CSymbol>(&t))			// unary operators
	{
		string s = pSymbol->SymbolSign;
		if (s == "+" || s == "-")
			return pSymbol->m_Fn(0, ParseNumOrExp(c));
		ThrowError("Invalid symbol:" + s);		
	}
		

	if (auto pWord = get_if<Word>(&t))
	{
		Token t = PeekToken(c.ts);
		if (HasType<LParen>(t))
			return Fns.Apply(pWord->Value, ParseFunctionParams(c));
		auto oResult = c.m.TryGet(pWord->Value);
		if (oResult)
			return *oResult;
		else
			return GetConstant(pWord->Value);
	}
	
	if (HasType<LParen>(t))
		return ParseParenRest(c);

	if (auto pNum = get_if<Num>(&t))
		return pNum->Value;

	ThrowError("ParseExpression");
}


Result ParseExpSym(CContext &c)
{
	auto NumLeft = ParseNumOrExp(c);
	for (;;)
	{
		auto tLeft = PeekToken(c.ts);
		if (!HasType<CSymbol>(tLeft))
			return NumLeft;
		CSymbol sLeft = TakeTypedToken<CSymbol>(c.ts);
		auto NumMiddle = ParseNumOrExp(c);
		auto tRight = PeekToken(c.ts);
		if (!HasType<CSymbol>(tRight))
			return sLeft.m_Fn(NumLeft, NumMiddle);
		CSymbol sRight = TakeTypedToken<CSymbol>(c.ts);
		auto NumRight = ParseNumOrExp(c);
		if (sLeft.m_Precedence >= sRight.m_Precedence)
			NumLeft = sRight.m_Fn(sLeft.m_Fn(NumLeft, NumMiddle), NumRight);
		else
			NumLeft = sLeft.m_Fn(NumLeft, sRight.m_Fn(NumMiddle, NumRight));
	}
}

Result ParseExpression(CContext &c)
{
	return ParseExpSym(c);
}

Result Parse(CContext &c)
{
	Result Res = ParseExpression(c);
	if (c.ts.size() != 1 || !HasType<End>(c.ts[0]))
		ThrowError("Unparsed rest");
	return Res;
}

Result ParseMemoryCmd(CContext &c, const string& Cmd)
{
	
	if (Cmd == "ms")
	{		
		TakeToken(c.ts);
		auto w = TakeTypedToken<Word>(c.ts);
		Result r = HasType<End>(PeekToken(c.ts)) ? c.m.LastResult : Parse(c);
		c.m.Add(w.Value, r);
		return r;
	}
	else if (Cmd == "mc")
	{
		TakeToken(c.ts);

		if (HasType<Word>(PeekToken(c.ts)))
		{
			c.m.Erase(TakeTypedToken<Word>(c.ts).Value);
		}
		else
			c.m.Clear();
		int Size = c.m.Size();
		
		return Size;
	}

	ThrowError("Invalid memory command : " + Cmd);
}

Result Eval(string_view s, CMemory& m)
{
	auto ts = Tokenize(s);	
	CContext c{ ts, m };

	// we are trying to guess the next 2 tokens
	auto ts2 = ts;
	if (auto pWord = GuessNextToken<Word>(ts2))
	{
		if (auto pAssign = GuessNextToken<Assignment>(ts2))
		{
			CContext c2{ ts2, m };
			c.ts = ts2;
			c.m.Add(pWord->Value, pAssign->Apply(c.m.Get(pWord->Value), ParseExpression(c2)));
		}
	}
/*

	if (auto pWord = GuessNextToken<Word>(ts))
	{
		if (pWord->Value[0] == 'm')
		{
			return ParseMemoryCmd(c, pWord->Value);
		}
	}*/
	return Parse(c);
}

void PrintTokenLine(const ETokenError &te, string_view Origin)
{
	if (te.Location().empty())
		return;
	string sSpaces(te.Location().data() - Origin.data(), ' ');
	string sEquals(te.Location().cend() - te.Location().cbegin(), '=');
	cout << sSpaces << sEquals << endl;
}

void ShowHelp()
{
	cout << "Functions : " << endl;
	cout << " --------   " << endl;

}

int main()
{

	ofstream of("results.txt");
	CMemory m;
	
	for (;;)
	{
		string s;
		getline(cin, s);
		if (s.empty())
			break;
		if (s == "?")
		{
			ShowHelp();
			continue;
		}
		of << s << " = ";
		try
		{
			m.LastResult = Eval(s, m);
			cout << " = " << m.LastResult << endl;
			of << m.LastResult << endl;
			if (m.Size())
				cout << "Memory: " << m.Dump() << endl;
		}
		catch (const std::exception& e)
		{
			if (auto* pTokenErr = dynamic_cast<const ETokenError*>(&e))
			{
				PrintTokenLine(*pTokenErr, s);
			}
			cerr << "Error : " << e.what() << endl;
			of << "[ERR]:" << e.what() << endl;
		}

	}

     return 0;
}





