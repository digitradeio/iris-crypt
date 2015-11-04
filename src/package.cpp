#include "package.hpp"
#include "base32.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <numeric>
#include <fstream>
#include <regex>

#pragma warning(push, 3)
#include <node_buffer.h>

#include <v8pp/class.hpp>
#include <v8pp/object.hpp>
#include <v8pp/call_v8.hpp>

#include <yas/binary_iarchive.hpp>
#include <yas/binary_oarchive.hpp>
#include <yas/mem_streams.hpp>
#include <yas/file_streams.hpp>
#include <yas/serializers/std_types_serializers.hpp>
#pragma warning(pop)

static uint32_t const SIGN = 0x30504349; // ICP0

v8::UniquePersistent<v8::Object> package::node_module;
v8::UniquePersistent<v8::Function> package::node_require;
v8::UniquePersistent<v8::Object> package::node_crypto;

static std::string pbkdf2(v8::Isolate* isolate, std::string const& password, std::string const& salt,
	size_t iterations, size_t keylen, char const* digest = "sha256")
{
	v8::HandleScope scope(isolate);

	v8::Local<v8::Object> crypto = v8pp::to_local(isolate, package::node_crypto);

	v8::Local<v8::Function> fun;
	v8pp::get_option(isolate, crypto, "pbkdf2Sync", fun);

	v8::Local<v8::Value> buf = v8pp::call_v8(isolate, fun, crypto, password, salt, iterations, keylen, digest);

	return std::string(node::Buffer::Data(buf), node::Buffer::Length(buf));
}

static std::string random_bytes(v8::Isolate* isolate, size_t size)
{
	v8::HandleScope scope(isolate);

	v8::Local<v8::Object> crypto = v8pp::to_local(isolate, package::node_crypto);

	v8::Local<v8::Function> fun;
	v8pp::get_option(isolate, crypto, "randomBytes", fun);

	v8::Local<v8::Value> buf = v8pp::call_v8(isolate, fun, crypto, size);

	return std::string(node::Buffer::Data(buf), node::Buffer::Length(buf));
}

static yas::shared_buffer do_crypto(v8::Isolate* isolate, char const* function,
	std::string const& priv_key, char const* data, size_t size)
{
	v8::HandleScope scope(isolate);

	v8::Local<v8::Object> crypto = v8pp::to_local(isolate, package::node_crypto);

	v8::Local<v8::Function> create;
	v8pp::get_option(isolate, crypto, function, create);

	v8::Local<v8::Object> cipher = v8pp::call_v8(isolate, create, crypto, "aes256", priv_key).As<v8::Object>();
	v8::Local<v8::Function> update, final;
	v8pp::get_option(isolate, cipher, "update", update);
	v8pp::get_option(isolate, cipher, "final", final);

	v8::Local<v8::Value> buf1 = v8pp::call_v8(isolate, update, cipher, node::Buffer::New(isolate, data, size));
	v8::Local<v8::Value> buf2 = v8pp::call_v8(isolate, final, cipher);

	char const* const buf1_data = node::Buffer::Data(buf1);
	char const* const buf2_data = node::Buffer::Data(buf2);
	size_t const buf1_size = node::Buffer::Length(buf1);
	size_t const buf2_size = node::Buffer::Length(buf2);

	yas::shared_buffer result(buf1_size + buf2_size);
	std::copy(buf1_data, buf1_data + buf1_size, result.data.get());
	std::copy(buf2_data, buf2_data + buf2_size, result.data.get() + buf1_size);

	return result;
}

static yas::shared_buffer encrypt(v8::Isolate* isolate, std::string const& priv_key, char const* data, size_t size)
{
	return do_crypto(isolate, "createCipher", priv_key, data, size);
}

static yas::shared_buffer decrypt(v8::Isolate* isolate, std::string const& priv_key, char const* data, size_t size)
{
	return do_crypto(isolate, "createDecipher", priv_key, data, size);
}

class auth_data
{
	// XXXX-XXXX-XXXX-XXXX-YYYY-ZZZZ
public:
	auth_data(v8::Isolate* isolate, uint16_t serial_number)
	{
		// XXXX
		data_ = pbkdf2(isolate, "irisCrypt", std::to_string(serial_number), 1000, 11);
		// YYYY
		data_.append((char*)&serial_number, sizeof(serial_number));
		// ZZZZ
		uint16_t const checksum = std::accumulate(data_.begin(), data_.end(), uint16_t{});
		data_.append((char*)&checksum, sizeof(checksum));
	}

	explicit auth_data(std::string str)
	{
		str.erase(std::remove(str.begin(), str.end(), '-'), str.end());
		data_ = base32::decode<base32::crockford>(str);
	}

	std::string to_string() const
	{
		std::string result = base32::encode<base32::crockford>(data_);
		for (auto pos = result.begin() + 4; pos < result.end();)
		{
			pos = result.insert(pos, '-') + 5;
		}
		return result;
	}

	uint16_t serial_number() const
	{
		uint16_t number;
		memcpy(&number, data_.data() + PRIV_KEY_LEN, sizeof(number));
		return number;
	}
	std::string pub_key_string() const { return to_string().substr(20); }

	std::string pub_key() const { return data_.substr(PRIV_KEY_LEN); }
	std::string priv_key() const { return data_.substr(0, PRIV_KEY_LEN); }
private:
	static size_t const PRIV_KEY_LEN = 11;
	static size_t const AUTH_LEN = PRIV_KEY_LEN + sizeof(uint16_t) * 2; // + serial + checksum

	std::string data_;
};

void package::gen_auth(v8::FunctionCallbackInfo<v8::Value> const& args)
{
	v8::Isolate* isolate = args.GetIsolate();

	uint16_t const serial = v8pp::from_v8<uint16_t>(isolate, args[0], 0);
	auth_data const auth(isolate, serial);

	args.GetReturnValue().Set(v8pp::to_v8(isolate, auth.to_string()));
}

void package::make(v8::FunctionCallbackInfo<v8::Value> const& args)
try
{
	v8::Isolate* isolate = args.GetIsolate();
	v8::HandleScope scope(isolate);

	using string_map = std::map<std::string, std::string>;

	auth_data const auth(v8pp::from_v8<std::string>(isolate, args[0]));
	auto const filename = v8pp::from_v8<std::string>(isolate, args[1]);
	auto const files = v8pp::from_v8<string_map>(isolate, args[2]);

	modules_map modules;
	for (auto const& file : files)
	{
		load_file(isolate, modules, file.first, file.second, {});
	}

	yas::mem_ostream mem(modules.size() * 40 * 1024);
	yas::binary_oarchive<yas::mem_ostream> content(mem, yas::no_header);
	content.serialize(modules);

	yas::intrusive_buffer const buf = mem.get_intrusive_buffer();
	yas::shared_buffer const cipher = encrypt(isolate, auth.priv_key(), buf.data, buf.size);

	yas::file_ostream file(filename.c_str(), yas::file_trunc);
	yas::binary_oarchive<yas::file_ostream> out(file, yas::no_header);
	out.serialize(SIGN, auth.pub_key(), cipher);
}
catch (yas::io_exception const& ex)
{
	throw std::runtime_error(std::string("Package write error: ") + ex.what());
}

void package::load(v8::FunctionCallbackInfo<v8::Value> const& args)
try
{
	v8::Isolate* isolate = args.GetIsolate();

	auth_data const auth(v8pp::from_v8<std::string>(isolate, args[0]));
	auto const filename = v8pp::from_v8<std::string>(isolate, args[1]);

	std::unique_ptr<package> pkg(new package);

	std::decay<decltype(SIGN)>::type sign;
	yas::shared_buffer cipher;

	yas::file_istream file(filename.c_str());
	yas::binary_iarchive<yas::file_istream> in(file, yas::no_header);
	in.serialize(sign, pkg->pub_key_, cipher);
	if (sign != SIGN)
	{
		throw std::runtime_error("Package invalid format");
	}
	if (pkg->pub_key_ != auth.pub_key())
	{
		throw std::runtime_error("Package invalid key");
	}
	pkg->pub_key_ = auth.pub_key_string();
	pkg->serial_number_ = auth.serial_number();

	yas::shared_buffer const plain = decrypt(isolate, auth.priv_key(), cipher.data.get(), cipher.size);
	yas::mem_istream mem(plain);
	yas::binary_iarchive<yas::mem_istream> content(mem, yas::no_header);
	content.serialize(pkg->modules_);

	pkg->js_modules_.Reset(isolate, v8::Object::New(isolate));
	v8::Local<v8::Object> result = v8pp::class_<package>::import_external(isolate, pkg.release());

	args.GetReturnValue().Set(result);
}
catch (yas::io_exception const& ex)
{
	throw std::runtime_error(std::string("Package read error: ") + ex.what());
}

std::vector<std::string> package::names() const
{
	// list only root items
	std::vector<std::string> result;
	for (auto const& kv : modules_)
	{
		if (kv.first.is_root())
		{
			result.emplace_back(kv.first.id);
		}
	}
	return result;
}

v8::Local<v8::Value> package::require_module(v8::Isolate* isolate, module_name const& name, std::string const& source)
{
	// re-define require() function in a wrapped source
	// because for some reason V8 can't reference it
	// from this package object prototype
	char const wrapper_begin[] = 
		"(function (exports, module, __filename, __dirname){"
		"var require = function(name) { return module.require(name) };";
	char const wrapper_end[] = "\n});";

	// wrap module.source into JavaScript (function(){}) to hide the module source code
	std::string wrapped_source;
	wrapped_source.reserve(source.length() + sizeof(wrapper_begin) + sizeof(wrapper_end) - 2);
	wrapped_source.append(wrapper_begin, sizeof(wrapper_begin) - 1);
	wrapped_source.append(source);
	wrapped_source.append(wrapper_end, sizeof(wrapper_end) - 1);

	v8::TryCatch try_catch;

	// compile and run wrapped source to get a wrapped JS function
	v8::ScriptOrigin origin(v8pp::to_v8(isolate, name.file));
	v8::Local<v8::Script> script = v8::Script::Compile(v8pp::to_v8(isolate, wrapped_source), &origin);
	if (try_catch.HasCaught())
	{
		try_catch.ReThrow();
		return v8::Undefined(isolate);
	}
	v8::Local<v8::Function> wrapped_script = script->Run().As<v8::Function>();
	if (try_catch.HasCaught())
	{
		try_catch.ReThrow();
		return v8::Undefined(isolate);
	}

	// create a module object and set it protoptype to this package
	v8::Local<v8::Object> js_module = v8::Object::New(isolate);
	js_module->SetPrototype(v8pp::to_v8(isolate, this));

	// setup the module object
	path const fullname = name.dir / name.file;
	v8::Local<v8::Object> exports = v8::Object::New(isolate);
	v8pp::set_option(isolate, js_module, "exports", exports);
	v8pp::set_const(isolate, js_module, "id", name.id);
	v8pp::set_const(isolate, js_module, "filename", fullname);
	v8pp::set_option(isolate, js_module, "loaded", false);

	// call wrapped function
	v8pp::call_v8(isolate, wrapped_script, js_module, exports,js_module, fullname, name.dir);
	if (try_catch.HasCaught())
	{
		try_catch.ReThrow();
		return v8::Undefined(isolate);
	}

	// get exports object
	v8pp::set_option(isolate, js_module, "loaded", true);
	v8pp::get_option(isolate, js_module, "exports", exports);
	return exports;
}

v8::Local<v8::Value> package::require_original(v8::Isolate* isolate, std::string const& name)
{
	// load node module using original require function
	v8::Local<v8::Object> module = v8pp::to_local(isolate, node_module);
	v8::Local<v8::Function> require = v8pp::to_local(isolate, node_require);
	v8::TryCatch try_catch;
	v8::Local<v8::Value> result = v8pp::call_v8(isolate, require, module, name);
	if (try_catch.HasCaught())
	{
		return try_catch.ReThrow();
	}
	return result;
}

void package::require(v8::FunctionCallbackInfo<v8::Value> const& args)
{
	v8::Isolate* isolate = args.GetIsolate();

	module_name name;
	name.id = v8pp::from_v8<std::string>(isolate, args[0], "");
	if (name.id.empty())
	{
		throw std::runtime_error("name argument empty");
	}
	if (!name.is_root() && !require_dir_stack_.empty())
	{
		name.dir = (require_dir_stack_.top() / name.id).parts().first;
	}

	auto it = modules_.find(name);
	if (it == modules_.end())
	{
		args.GetReturnValue().Set(require_original(isolate, name.id));
		return;
	}
	name = it->first;
	std::string& source = it->second;

	v8::EscapableHandleScope scope(isolate);

	v8::Local<v8::Object> js_modules = v8pp::to_local(isolate, js_modules_);
	v8::Local<v8::String> js_name = v8pp::to_v8(isolate, name.dir / name.id);
	v8::Local<v8::Value> js_module = js_modules->Get(js_name);
	if (js_module.IsEmpty() || js_module->IsUndefined())
	{
		if (name.file.extension() == ".json")
		{
			js_module = v8::JSON::Parse(v8pp::to_v8(isolate, source));
		}
		else
		{
			require_dir_stack_.push((name.dir / name.file).parent());
			js_module = require_module(isolate, name, source);
			require_dir_stack_.pop();
		}
		source.clear();
		source.shrink_to_fit();
		js_modules->Set(js_name, js_module);
	}
	args.GetReturnValue().Set(scope.Escape(js_module));
}

void package::load_file(v8::Isolate* isolate, modules_map& modules, std::string const& id, path p, path base)
{
	if (base.empty()) base = p.parts().first;

	if (p.is_dir())
	{
		path const package = p / "package.json";
		if (package.is_file())
		{
			v8::HandleScope scope(isolate);

			v8::Local<v8::Function> require = v8pp::to_local(isolate, node_require);
			v8::Local<v8::Object> json = v8pp::call_v8(isolate, require,
				v8pp::to_local(isolate, node_module), package).As<v8::Object>();
			if (json.IsEmpty() || !json->IsObject())
			{
				throw std::runtime_error(id + ": can't load " + package.str());
			}
			path main;
			if (!v8pp::get_option(isolate, json, "main", main))
			{
				throw std::runtime_error(id + ": no \"main\" in " + package.str());
			}
			p = p / main;
		}
		else
		{
			p = p / "index.js";
		}
	}

	if (p.extension().empty())
	{
		p.set_extension(".js");
	}

	std::ifstream file(p.c_str());
	if (!file.is_open())
	{
		throw std::runtime_error(id + ": can't open " + p.str());
	}
	std::istreambuf_iterator<char> begin(file), end;
	std::string const source(begin, end);

	module_name name;
	name.id = id;
	if (name.is_root())
	{
		name.file = p.relative_to(base);
	}
	else
	{
		std::tie(name.dir, name.file) = p.relative_to(base).parts();
	}
	modules.emplace(name, source);

	p = p.parent();
	std::regex re("require\\([\"\'](.*?[\\/\\/].*?)[\"\']\\)");
	for (std::sregex_iterator it(source.begin(), source.end(), re), end; it != end; ++it)
	{
		std::string const& id = (*it)[1];
		load_file(isolate, modules, id, p / id, base);
	}
}