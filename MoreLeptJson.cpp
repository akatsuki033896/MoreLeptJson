#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>
#include <regex>
#include <charconv> // from_chars() string -> float/int
#include "Print.h"

struct JSONObject;

using JSONList = std::vector<JSONObject>;
using JSONDict = std::unordered_map<std::string, JSONObject>;

struct JSONObject {
	std::variant
		<std::nullptr_t // null
		, bool
		, int
		, double
		, std::string
		, JSONList // [1, "a"]
		, JSONDict // {1: "a"}
		> inner;

	void do_print() const {
		print(inner);
	}

	template <class T>
	bool is() const {
		return std::holds_alternative<T>(inner);
	}

	template <class T>
	T const& get() const {
		return std::get<T>(inner);
	}

	template <class T>
	T &get() const {
		return std::get<T>(inner);
	}
};

template <class T>
std::optional<T> try_parse_num(std::string_view str) {
	T value;
	auto res = std::from_chars(str.data(), str.data() + str.size(), value); //str首地址，尾地址， base（默认为10）返回值from_chars_result结构体
	if (res.ec == std::errc() && res.ptr == str.data() + str.size())//根据指针位置，判断之后是否有不需要的字符
		return value;
	return std::nullopt;
}//简化版variant,只有T和nullopt

char unescaped_char(char c) {
	switch (c) {
	case 'n': return '\n';
	case 'r': return '\r';
	case '0': return '\0';
	case 't': return '\t';
	case 'v': return '\v';
	case 'f': return '\f';
	case 'b': return '\b';
	case 'a': return '\a';
	default: return c;
	}
}

std::pair<JSONObject, size_t> parse(std::string_view json) {
	if (json.empty())
		return { JSONObject{ std::nullptr_t{} }, 0 };
	else if (size_t off = json.find_first_not_of("\n\r\t\v\f\0"); off != 0 && off != json.npos) {
		auto [obj, eaten] = parse(json.substr(off));
		return { std::move(obj), eaten + off };
	}
	//parse number
	else if (json[0] >= '0' && json[0] <= '9' || json[0] == '+' || json[0] == '-') {
		std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
		std::cmatch match;
		if (std::regex_search(json.data(), json.data() + json.size(), match, num_re)) {
			std::string str = match.str();
			if (auto num = try_parse_num<int>(str); num.has_value())
				return { JSONObject{ num.value() }, str.size() }; //str.size():吃掉了几个字符
			if (auto num = try_parse_num<double>(str); num.has_value())
				return { JSONObject{ num.value() }, str.size() };
		}
	}
	// parse string
	else if (json[0] == '"') {
		std::string str;
		enum {
			Raw,
			Escaped,
		}phase = Raw; //初始化为Raw
		size_t i;
		for (i = 1; i < json.size(); i++) {
			char ch = json[i];
			if (phase == Raw) {
				if (ch == '\\')
					phase = Escaped; //转义模式 不输出
				else if (ch == '"') {
					i++;
					break;
				}
				else
					str += ch;
			}
			else if (phase == Escaped) {
				str += unescaped_char(ch);
				phase = Raw;
			}
		}
		return { JSONObject{ std::move(str) }, i };
	}
	//parse list
	else if (json[0] == '[') {
		std::vector<JSONObject> res;
		size_t i;
		for (i = 1; i < json.size(); i++) {
			if (json[i] == ']') {
				i++;
				break;
			}
			auto [obj, eaten] = parse(json.substr(i)); //对象,被吃掉的字符
			if (eaten == 0) {
				i = 0;
				break;
			}
			res.push_back(std::move(obj));
			i += eaten;
			if (json[i] == ',')
				i++;
		}
		return { JSONObject{ std::move(res) }, i };
	}
	//parse dict
	else if (json[0] == '{') {
		std::unordered_map<std::string, JSONObject> res;
		size_t i;
		for (i = 1; i < json.size(); i++) {
			if (json[i] == '}') {
				i++;
				break;
			}
			auto [keyobj, keyeaten] = parse(json.substr(i));
			if (keyeaten == 0) {
				i = 0;
				break;
			}
			i += keyeaten;
			if (!std::holds_alternative<std::string>(keyobj.inner)) {
				i = 0;
				break;
			}// 是否string
			if (json[i] == ':')
				i++;
			std::string key = std::move(std::get<std::string>(keyobj.inner));
			auto [valobj, valeaten] = parse(json.substr(i));
			if (valeaten == 0) {
				i == 0;
				break;
			}
			i += valeaten;
			res[key] = valobj;
			if (json[i] == ',')
				i++;
		}
		return { JSONObject{std::move(res)}, i };
	}
	return { JSONObject{ std::nullptr_t{} }, 0 };
}

//struct Functor {
//	void operator()(int val) const {
//		print("int is:", val);
//	}
//	void operator()(double val) const {
//		print("double is:", val);
//	}
//	void operator()(std::string val) const {
//		print("string is:", val);
//	}
//	template <class T>
//	void operator()(T val) const {
//		print("Unknown object is:", val);
//	}
//};

template <class ...Fs>
struct overloaded : Fs... {
	using Fs::operator()...;
};

template <class ...Fs>
overloaded(Fs...) -> overloaded<Fs...>;

int main()
{
	std::string_view str = R"JSON(985.211)JSON";

	auto [obj, eaten] = parse(str);
	print(obj);
	//std::visit(Functor(), obj.inner);
	std::visit(
		overloaded{
			[&](int val) {
				print("int is:", val);
			},
			[&](double val) {
				print("double is:", val);
			},
			[&](std::string val) {
				print("string is:", val);
			},
			[&](auto val) {
				print("unknown object is:", val);
			},
		},
		obj.inner);
	return 0;
}
