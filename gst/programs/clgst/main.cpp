/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 *  @defgroup gstclienttool GSTIO Command Line Client Reference
 *  @brief Tool for sending transactions and querying state from @ref nodgst
 *  @ingroup gstclienttool
 */

 /**
   @defgroup gstclienttool

   @section intro Introduction to clgst

   `clgst` is a command line tool that interfaces with the REST api exposed by @ref nodgst. In order to use `clgst` you will need to
   have a local copy of `nodgst` running and configured to load the 'gstio::chain_api_plugin'.

	clgst contains documentation for all of its commands. For a list of all commands known to clgst, simply run it with no arguments:
 ```
 $ ./clgst
 Command Line Interface to GSTIO Client
 Usage: programs/clgst/clgst [OPTIONS] SUBCOMMAND

 Options:
   -h,--help                   Print this help message and exit
   -u,--url TEXT=http://localhost:8888/
							   the http/https URL where nodgst is running
   --wallet-url TEXT=http://localhost:8888/
							   the http/https URL where kgstd is running
   -r,--header                 pass specific HTTP header, repeat this option to pass multiple headers
   -n,--no-verify              don't verify peer certificate when using HTTPS
   -v,--verbose                output verbose actions on error

 Subcommands:
   version                     Retrieve version information
   create                      Create various items, on and off the blockchain
   get                         Retrieve various items and information from the blockchain
   set                         Set or update blockchain state
   transfer                    Transfer GST from account to account
   net                         Interact with local p2p network connections
   wallet                      Interact with local wallet
   sign                        Sign a transaction
   push                        Push arbitrary transactions to the blockchain
   multisig                    Multisig contract commands

 ```
 To get help with any particular subcommand, run it with no arguments as well:
 ```
 $ ./clgst create
 Create various items, on and off the blockchain
 Usage: ./clgst create SUBCOMMAND

 Subcommands:
   key                         Create a new keypair and print the public and private keys
   account                     Create a new account on the blockchain (assumes system contract does not restrict RAM usage)

 $ ./clgst create account
 Create a new account on the blockchain (assumes system contract does not restrict RAM usage)
 Usage: ./clgst create account [OPTIONS] creator name OwnerKey ActiveKey

 Positionals:
   creator TEXT                The name of the account creating the new account
   name TEXT                   The name of the new account
   OwnerKey TEXT               The owner public key for the new account
   ActiveKey TEXT              The active public key for the new account

 Options:
   -x,--expiration             set the time in seconds before a transaction expires, defaults to 30s
   -f,--force-unique           force the transaction to be unique. this will consume extra bandwidth and remove any protections against accidently issuing the same transaction multiple times
   -s,--skip-sign              Specify if unlocked wallet keys should be used to sign transaction
   -d,--dont-broadcast         don't broadcast transaction to the network (just print to stdout)
   -p,--permission TEXT ...    An account and permission level to authorize, as in 'account@permission' (defaults to 'creator@active')
 ```
 */

#include <pwd.h>
#include <string>
#include <vector>
#include <regex>
#include <iostream>
#include <fc/crypto/hex.hpp>
#include <fc/variant.hpp>
#include <fc/io/datastream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/console.hpp>
#include <fc/exception/exception.hpp>
#include <fc/variant_object.hpp>
#include <gstio/utilities/key_conversion.hpp>

#include <gstio/chain/name.hpp>
#include <gstio/chain/config.hpp>
#include <gstio/chain/wast_to_wasm.hpp>
#include <gstio/chain/trace.hpp>
#include <gstio/chain_plugin/chain_plugin.hpp>
#include <gstio/chain/contract_types.hpp>

#pragma push_macro("N")
#undef N

#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/spawn.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/algorithm/string/classification.hpp>

#pragma pop_macro("N")

#include <Inline/BasicTypes.h>
#include <IR/Module.h>
#include <IR/Validate.h>
#include <WASM/WASM.h>
#include <Runtime/Runtime.h>

#include <fc/io/fstream.hpp>

#include "CLI11.hpp"
#include "help_text.hpp"
#include "localize.hpp"
#include "config.hpp"
#include "httpc.hpp"

using namespace std;
using namespace gstio;
using namespace gstio::chain;
using namespace gstio::utilities;
using namespace gstio::client::help;
using namespace gstio::client::http;
using namespace gstio::client::localize;
using namespace gstio::client::config;
using namespace boost::filesystem;

FC_DECLARE_EXCEPTION(explained_exception, 9000000, "explained exception, see error log");
FC_DECLARE_EXCEPTION(localized_exception, 10000000, "an error occured");
#define GSTC_ASSERT( TEST, ... ) \
  FC_EXPAND_MACRO( \
    FC_MULTILINE_MACRO_BEGIN \
      if( UNLIKELY(!(TEST)) ) \
      {                                                   \
        std::cerr << localized( __VA_ARGS__ ) << std::endl;  \
        FC_THROW_EXCEPTION( explained_exception, #TEST ); \
      }                                                   \
    FC_MULTILINE_MACRO_END \
  )

//copy pasta from kgstd's main.cpp
bfs::path determine_home_directory()
{
	bfs::path home;
	struct passwd* pwd = getpwuid(getuid());
	if (pwd) {
		home = pwd->pw_dir;
	}
	else {
		home = getenv("HOME");
	}
	if (home.empty())
		home = "./";
	return home;
}

string url = "http://127.0.0.1:8888/";
//string default_wallet_url = "unix://" + (determine_home_directory() / "gstio-wallet" / (string(key_store_executable_name) + ".sock")).string();
string default_wallet_url = "unix:///work/gst_install/wallet/" + string(key_store_executable_name) + ".sock";
string wallet_url; //to be set to default_wallet_url in main
bool no_verify = false;
vector<string> headers;

auto   tx_expiration = fc::seconds(30);
const fc::microseconds abi_serializer_max_time = fc::seconds(10); // No risk to client side serialization taking a long time
string tx_ref_block_num_or_id;
bool   tx_force_unique = false;
bool   tx_dont_broadcast = false;
bool   tx_return_packed = false;
bool   tx_skip_sign = false;
bool   tx_print_json = false;
bool   print_request = false;
bool   print_response = false;
bool   no_auto_kgstd = false;

uint8_t  tx_max_cpu_usage = 0;
uint32_t tx_max_net_usage = 0;

uint32_t delaysec = 0;

vector<string> tx_permission;

gstio::client::http::http_context context;

void add_standard_transaction_options(CLI::App* cmd, string default_permission = "") {
	CLI::callback_t parse_expiration = [](CLI::results_t res) -> bool {
		double value_s;
		if (res.size() == 0 || !CLI::detail::lexical_cast(res[0], value_s)) {
			return false;
		}

		tx_expiration = fc::seconds(static_cast<uint64_t>(value_s));
		return true;
	};

	cmd->add_option("-x,--expiration", parse_expiration, localized("set the time in seconds before a transaction expires, defaults to 30s"));
	cmd->add_flag("-f,--force-unique", tx_force_unique, localized("force the transaction to be unique. this will consume extra bandwidth and remove any protections against accidently issuing the same transaction multiple times"));
	cmd->add_flag("-s,--skip-sign", tx_skip_sign, localized("Specify if unlocked wallet keys should be used to sign transaction"));
	cmd->add_flag("-j,--json", tx_print_json, localized("print result as json"));
	cmd->add_flag("-d,--dont-broadcast", tx_dont_broadcast, localized("don't broadcast transaction to the network (just print to stdout)"));
	cmd->add_flag("--return-packed", tx_return_packed, localized("used in conjunction with --dont-broadcast to get the packed transaction"));
	cmd->add_option("-r,--ref-block", tx_ref_block_num_or_id, (localized("set the reference block num or block id used for TAPOS (Transaction as Proof-of-Stake)")));

	string msg = "An account and permission level to authorize, as in 'account@permission'";
	if (!default_permission.empty())
		msg += " (defaults to '" + default_permission + "')";
	cmd->add_option("-p,--permission", tx_permission, localized(msg.c_str()));

	cmd->add_option("--max-cpu-usage-ms", tx_max_cpu_usage, localized("set an upper limit on the milliseconds of cpu usage budget, for the execution of the transaction (defaults to 0 which means no limit)"));
	cmd->add_option("--max-net-usage", tx_max_net_usage, localized("set an upper limit on the net usage budget, in bytes, for the transaction (defaults to 0 which means no limit)"));

	cmd->add_option("--delay-sec", delaysec, localized("set the delay_sec seconds, defaults to 0s"));
}

vector<chain::permission_level> get_account_permissions(const vector<string>& permissions) {
	auto fixedPermissions = permissions | boost::adaptors::transformed([](const string& p) {
		vector<string> pieces;
		split(pieces, p, boost::algorithm::is_any_of("@"));
		if (pieces.size() == 1) pieces.push_back("active");
		return chain::permission_level{ .actor = pieces[0],.permission = pieces[1] };
	});
	vector<chain::permission_level> accountPermissions;
	boost::range::copy(fixedPermissions, back_inserter(accountPermissions));
	return accountPermissions;
}

template<typename T>
fc::variant call(const std::string& url,
	const std::string& path,
	const T& v) {
	try {
		auto sp = std::make_unique<gstio::client::http::connection_param>(context, parse_url(url) + path, no_verify ? false : true, headers);
		return gstio::client::http::do_http_call(*sp, fc::variant(v), print_request, print_response);
	}
	catch (boost::system::system_error& e) {
		if (url == ::url)
			std::cerr << localized("Failed to connect to nodgst at ${u}; is nodgst running?", ("u", url)) << std::endl;
		else if (url == ::wallet_url)
			std::cerr << localized("Failed to connect to kgstd at ${u}; is kgstd running?", ("u", url)) << std::endl;
		throw connection_exception(fc::log_messages{ FC_LOG_MESSAGE(error, e.what()) });
	}
}

template<typename T>
fc::variant call(const std::string& path,
	const T& v) {
	return call(url, path, fc::variant(v));
}

template<>
fc::variant call(const std::string& url,
	const std::string& path) {
	return call(url, path, fc::variant());
}

gstio::chain_apis::read_only::get_info_results get_info() {
	return call(url, get_info_func).as<gstio::chain_apis::read_only::get_info_results>();
}

string generate_nonce_string() {
	return fc::to_string(fc::time_point::now().time_since_epoch().count());
}

chain::action generate_nonce_action() {
	return chain::action({}, config::null_account_name, "nonce", fc::raw::pack(fc::time_point::now().time_since_epoch().count()));
}

void prompt_for_wallet_password(string& pw, const string& name) {
	if (pw.size() == 0 && name != "SecureEnclave") {
		std::cout << localized("password: ");
		fc::set_console_echo(false);
		std::getline(std::cin, pw, '\n');
		fc::set_console_echo(true);
	}
}

fc::variant determine_required_keys(const signed_transaction& trx) {
	// TODO better error checking
	//wdump((trx));
	const auto& public_keys = call(wallet_url, wallet_public_keys);
	auto get_arg = fc::mutable_variant_object
	("transaction", (transaction)trx)
		("available_keys", public_keys);
	const auto& required_keys = call(get_required_keys, get_arg);
	return required_keys["required_keys"];
}

void sign_transaction(signed_transaction& trx, fc::variant& required_keys, const chain_id_type& chain_id) {
	fc::variants sign_args = { fc::variant(trx), required_keys, fc::variant(chain_id) };
	const auto& signed_trx = call(wallet_url, wallet_sign_trx, sign_args);
	trx = signed_trx.as<signed_transaction>();
}

fc::variant push_transaction(signed_transaction& trx, int32_t extra_kcpu = 1000, packed_transaction::compression_type compression = packed_transaction::none) {
	auto info = get_info();

	if (trx.signatures.size() == 0) { // #5445 can't change txn content if already signed
		trx.expiration = info.head_block_time + tx_expiration;

		// Set tapos, default to last irreversible block if it's not specified by the user
		block_id_type ref_block_id = info.last_irreversible_block_id;
		try {
			fc::variant ref_block;
			if (!tx_ref_block_num_or_id.empty()) {
				ref_block = call(get_block_func, fc::mutable_variant_object("block_num_or_id", tx_ref_block_num_or_id));
				ref_block_id = ref_block["id"].as<block_id_type>();
			}
		} GST_RETHROW_EXCEPTIONS(invalid_ref_block_exception, "Invalid reference block num or id: ${block_num_or_id}", ("block_num_or_id", tx_ref_block_num_or_id));
		trx.set_reference_block(ref_block_id);

		if (tx_force_unique) {
			trx.context_free_actions.emplace_back(generate_nonce_action());
		}

		trx.max_cpu_usage_ms = tx_max_cpu_usage;
		trx.max_net_usage_words = (tx_max_net_usage + 7) / 8;
		trx.delay_sec = delaysec;
	}

	if (!tx_skip_sign) {
		auto required_keys = determine_required_keys(trx);
		sign_transaction(trx, required_keys, info.chain_id);
	}

	if (!tx_dont_broadcast) {
		return call(push_txn_func, packed_transaction(trx, compression));
	}
	else {
		if (!tx_return_packed) {
			return fc::variant(trx);
		}
		else {
			return fc::variant(packed_transaction(trx, compression));
		}
	}
}

fc::variant push_actions(std::vector<chain::action>&& actions, int32_t extra_kcpu, packed_transaction::compression_type compression = packed_transaction::none) {
	signed_transaction trx;
	trx.actions = std::forward<decltype(actions)>(actions);

	return push_transaction(trx, extra_kcpu, compression);
}

void print_action(const fc::variant& at) {
	const auto& receipt = at["receipt"];
	auto receiver = receipt["receiver"].as_string();
	const auto& act = at["act"].get_object();
	auto code = act["account"].as_string();
	auto func = act["name"].as_string();
	auto args = fc::json::to_string(act["data"]);
	auto console = at["console"].as_string();

	/*
	if( code == "gstio" && func == "setcode" )
	   args = args.substr(40)+"...";
	if( name(code) == config::system_account_name && func == "setabi" )
	   args = args.substr(40)+"...";
	*/
	if (args.size() > 100) args = args.substr(0, 100) + "...";
	cout << "#" << std::setw(14) << right << receiver << " <= " << std::setw(28) << std::left << (code + "::" + func) << " " << args << "\n";
	if (console.size()) {
		std::stringstream ss(console);
		string line;
		std::getline(ss, line);
		cout << ">> " << line << "\n";
	}
}

//resolver for ABI serializer to decode actions in proposed transaction in multisig contract
auto abi_serializer_resolver = [](const name& account) -> optional<abi_serializer> {
	static unordered_map<account_name, optional<abi_serializer> > abi_cache;
	auto it = abi_cache.find(account);
	if (it == abi_cache.end()) {
		auto result = call(get_abi_func, fc::mutable_variant_object("account_name", account));
		auto abi_results = result.as<gstio::chain_apis::read_only::get_abi_results>();

		optional<abi_serializer> abis;
		if (abi_results.abi.valid()) {
			abis.emplace(*abi_results.abi, abi_serializer_max_time);
		}
		else {
			std::cerr << "ABI for contract " << account.to_string() << " not found. Action data will be shown in hex only." << std::endl;
		}
		abi_cache.emplace(account, abis);

		return abis;
	}

	return it->second;
};

bytes variant_to_bin(const account_name& account, const action_name& action, const fc::variant& action_args_var) {
	auto abis = abi_serializer_resolver(account);
	FC_ASSERT(abis.valid(), "No ABI found for ${contract}", ("contract", account));

	auto action_type = abis->get_action_type(action);
	FC_ASSERT(!action_type.empty(), "Unknown action ${action} in contract ${contract}", ("action", action)("contract", account));
	return abis->variant_to_binary(action_type, action_args_var, abi_serializer_max_time);
}

fc::variant bin_to_variant(const account_name& account, const action_name& action, const bytes& action_args) {
	auto abis = abi_serializer_resolver(account);
	FC_ASSERT(abis.valid(), "No ABI found for ${contract}", ("contract", account));

	auto action_type = abis->get_action_type(action);
	FC_ASSERT(!action_type.empty(), "Unknown action ${action} in contract ${contract}", ("action", action)("contract", account));
	return abis->binary_to_variant(action_type, action_args, abi_serializer_max_time);
}

fc::variant json_from_file_or_string(const string& file_or_str, fc::json::parse_type ptype = fc::json::legacy_parser)
{
	regex r("^[ \t]*[\{\[]");
	if (!regex_search(file_or_str, r) && fc::is_regular_file(file_or_str)) {
		return fc::json::from_file(file_or_str, ptype);
	}
	else {
		return fc::json::from_string(file_or_str, ptype);
	}
}

bytes json_or_file_to_bin(const account_name& account, const action_name& action, const string& data_or_filename) {
	fc::variant action_args_var;
	if (!data_or_filename.empty()) {
		try {
			action_args_var = json_from_file_or_string(data_or_filename, fc::json::relaxed_parser);
		} GST_RETHROW_EXCEPTIONS(action_type_exception, "Fail to parse action JSON data='${data}'", ("data", data_or_filename));
	}
	return variant_to_bin(account, action, action_args_var);
}

void print_action_tree(const fc::variant& action) {
	print_action(action);
	const auto& inline_traces = action["inline_traces"].get_array();
	for (const auto& t : inline_traces) {
		print_action_tree(t);
	}
}

void print_result(const fc::variant& result) {
	try {
		if (result.is_object() && result.get_object().contains("processed")) {
			const auto& processed = result["processed"];
			const auto& transaction_id = processed["id"].as_string();
			string status = processed["receipt"].is_object() ? processed["receipt"]["status"].as_string() : "failed";
			int64_t net = -1;
			int64_t cpu = -1;
			if (processed.get_object().contains("receipt")) {
				const auto& receipt = processed["receipt"];
				if (receipt.is_object()) {
					net = receipt["net_usage_words"].as_int64() * 8;
					cpu = receipt["cpu_usage_us"].as_int64();
				}
			}

			cerr << status << " transaction: " << transaction_id << "  ";
			if (net < 0) {
				cerr << "<unknown>";
			}
			else {
				cerr << net;
			}
			cerr << " bytes  ";
			if (cpu < 0) {
				cerr << "<unknown>";
			}
			else {
				cerr << cpu;
			}

			cerr << " us\n";

			if (status == "failed") {
				auto soft_except = processed["except"].as<optional<fc::exception>>();
				if (soft_except) {
					edump((soft_except->to_detail_string()));
				}
			}
			else {
				const auto& actions = processed["action_traces"].get_array();
				for (const auto& a : actions) {
					print_action_tree(a);
				}
				wlog("\rwarning: transaction executed locally, but may not be confirmed by the network yet");
			}
		}
		else {
			cerr << fc::json::to_pretty_string(result) << endl;
		}
	} FC_CAPTURE_AND_RETHROW((result))
}

using std::cout;
void send_actions(std::vector<chain::action>&& actions, int32_t extra_kcpu = 1000, packed_transaction::compression_type compression = packed_transaction::none) {
	auto result = push_actions(move(actions), extra_kcpu, compression);

	if (tx_print_json) {
		cout << fc::json::to_pretty_string(result) << endl;
	}
	else {
		print_result(result);
	}
}

void send_transaction(signed_transaction& trx, int32_t extra_kcpu, packed_transaction::compression_type compression = packed_transaction::none) {
	auto result = push_transaction(trx, extra_kcpu, compression);

	if (tx_print_json) {
		cout << fc::json::to_pretty_string(result) << endl;
	}
	else {
		print_result(result);
	}
}

chain::action create_newaccount(const name& creator, const name& newaccount, public_key_type owner, public_key_type active) {
	return action{
	   tx_permission.empty() ? vector<chain::permission_level>{ {creator,config::active_name}} : get_account_permissions(tx_permission),
	   gstio::chain::newaccount{
		  .creator = creator,
		  .name = newaccount,
		  .owner = gstio::chain::authority{1, {{owner, 1}}, {}},
		  .active = gstio::chain::authority{1, {{active, 1}}, {}}
	   }
	};
}

chain::action create_action(const vector<permission_level>& authorization, const account_name& code, const action_name& act, const fc::variant& args) {
	return chain::action{ authorization, code, act, variant_to_bin(code, act, args) };
}




chain::action create_buymembytes(const name& creator, const name& newaccount, uint32_t numbytes) {
	fc::variant act_payload = fc::mutable_variant_object()
		("payer", creator.to_string())
		("receiver", newaccount.to_string())
		("bytes", numbytes);
	return create_action(tx_permission.empty() ? vector<chain::permission_level>{ {creator, config::active_name}} : get_account_permissions(tx_permission),
		config::system_account_name, N(buymembytes), act_payload);
}

chain::action create_buymem(const name& creator, const name& newaccount, const asset& quantity) {
	fc::variant act_payload = fc::mutable_variant_object()
		("payer", creator.to_string())
		("receiver", newaccount.to_string())
		("quant", quantity.to_string());
	return create_action(tx_permission.empty() ? vector<chain::permission_level>{ {creator, config::active_name}} : get_account_permissions(tx_permission),
		config::system_account_name, N(buymem), act_payload);
}








chain::action create_buyram(const name& creator, const name& newaccount, const asset& quantity) {
	fc::variant act_payload = fc::mutable_variant_object()
		("payer", creator.to_string())
		("receiver", newaccount.to_string())
		("quant", quantity.to_string());
	return create_action(tx_permission.empty() ? vector<chain::permission_level>{ {creator, config::active_name}} : get_account_permissions(tx_permission),
		config::system_account_name, N(buyram), act_payload);
}

chain::action create_buyrambytes(const name& creator, const name& newaccount, uint32_t numbytes) {
	fc::variant act_payload = fc::mutable_variant_object()
		("payer", creator.to_string())
		("receiver", newaccount.to_string())
		("bytes", numbytes);
	return create_action(tx_permission.empty() ? vector<chain::permission_level>{ {creator, config::active_name}} : get_account_permissions(tx_permission),
		config::system_account_name, N(buyrambytes), act_payload);
}

chain::action create_delegate(const name& from, const name& receiver, const asset& net, const asset& cpu, bool transfer) {
	fc::variant act_payload = fc::mutable_variant_object()
		("from", from.to_string())
		("receiver", receiver.to_string())
		("stake_net_quantity", net.to_string())
		("stake_cpu_quantity", cpu.to_string())
		("transfer", transfer);
	return create_action(tx_permission.empty() ? vector<chain::permission_level>{ {from, config::active_name}} : get_account_permissions(tx_permission),
		config::system_account_name, N(delegatebw), act_payload);
}


////
////
////
chain::action create_delegate_account(const name& from, const name& receiver, const asset& net, const asset& cpu, bool transfer) {
	fc::variant act_payload = fc::mutable_variant_object()
		("from", from.to_string())
		("receiver", receiver.to_string())
		("stake_net_quantity", net.to_string())
		("stake_cpu_quantity", cpu.to_string())
		("transfer", transfer);
	return create_action(tx_permission.empty() ? vector<chain::permission_level>{ {from, config::active_name}} : get_account_permissions(tx_permission),
		config::system_account_name, N(mydelegatebw), act_payload);
}
////
////
////


fc::variant regproducer_variant(const account_name& producer, const public_key_type& key, const string& url, uint16_t location) {
	return fc::mutable_variant_object()
		("producer", producer)
		("producer_key", key)
		("url", url)
		("location", location)
		;
}

chain::action create_open(const string& contract, const name& owner, symbol sym, const name& ram_payer) {
	auto open_ = fc::mutable_variant_object
	("owner", owner)
		("symbol", sym)
		("ram_payer", ram_payer);
	return action{
	  tx_permission.empty() ? vector<chain::permission_level>{ {ram_payer,config::active_name}} : get_account_permissions(tx_permission),
	  contract, "open", variant_to_bin(contract, N(open), open_)
	};
}

chain::action create_transfer(const string& contract, const name& sender, const name& recipient, asset amount, const string& memo) {

	auto transfer = fc::mutable_variant_object
	("from", sender)
		("to", recipient)
		("quantity", amount)
		("memo", memo);

	return action{
	   tx_permission.empty() ? vector<chain::permission_level>{ {sender,config::active_name}} : get_account_permissions(tx_permission),
	   contract, "transfer", variant_to_bin(contract, N(transfer), transfer)
	};
}

chain::action create_setabi(const name& account, const bytes& abi) {
	return action{
	   tx_permission.empty() ? vector<chain::permission_level>{ {account,config::active_name}} : get_account_permissions(tx_permission),
	   setabi{
		  .account = account,
		  .abi = abi
	   }
	};
}

chain::action create_setcode(const name& account, const bytes& code) {
	return action{
	   tx_permission.empty() ? vector<chain::permission_level>{ {account,config::active_name}} : get_account_permissions(tx_permission),
	   setcode{
		  .account = account,
		  .vmtype = 0,
		  .vmversion = 0,
		  .code = code
	   }
	};
}

chain::action create_updateauth(const name& account, const name& permission, const name& parent, const authority& auth) {
	return action{ tx_permission.empty() ? vector<chain::permission_level>{ {account,config::active_name}} : get_account_permissions(tx_permission),
					updateauth{account, permission, parent, auth} };
}

chain::action create_deleteauth(const name& account, const name& permission) {
	return action{ tx_permission.empty() ? vector<chain::permission_level>{ {account,config::active_name}} : get_account_permissions(tx_permission),
					deleteauth{account, permission} };
}

chain::action create_linkauth(const name& account, const name& code, const name& type, const name& requirement) {
	return action{ tx_permission.empty() ? vector<chain::permission_level>{ {account,config::active_name}} : get_account_permissions(tx_permission),
					linkauth{account, code, type, requirement} };
}

chain::action create_unlinkauth(const name& account, const name& code, const name& type) {
	return action{ tx_permission.empty() ? vector<chain::permission_level>{ {account,config::active_name}} : get_account_permissions(tx_permission),
					unlinkauth{account, code, type} };
}

authority parse_json_authority(const std::string& authorityJsonOrFile) {
	try {
		return json_from_file_or_string(authorityJsonOrFile).as<authority>();
	} GST_RETHROW_EXCEPTIONS(authority_type_exception, "Fail to parse Authority JSON '${data}'", ("data", authorityJsonOrFile))
}

authority parse_json_authority_or_key(const std::string& authorityJsonOrFile) {
	if (boost::istarts_with(authorityJsonOrFile, "GST") || boost::istarts_with(authorityJsonOrFile, "PUB_R1")) {
		try {
			return authority(public_key_type(authorityJsonOrFile));
		} GST_RETHROW_EXCEPTIONS(public_key_type_exception, "Invalid public key: ${public_key}", ("public_key", authorityJsonOrFile))
	}
	else {
		auto result = parse_json_authority(authorityJsonOrFile);
		GST_ASSERT(gstio::chain::validate(result), authority_type_exception, "Authority failed validation! ensure that keys, accounts, and waits are sorted and that the threshold is valid and satisfiable!");
		return result;
	}
}

asset to_asset(account_name code, const string& s) {
	static map< pair<account_name, gstio::chain::symbol_code>, gstio::chain::symbol> cache;
	auto a = asset::from_string(s);
	gstio::chain::symbol_code sym = a.get_symbol().to_symbol_code();
	auto it = cache.find(make_pair(code, sym));
	auto sym_str = a.symbol_name();
	if (it == cache.end()) {
		auto json = call(get_currency_stats_func, fc::mutable_variant_object("json", false)
			("code", code)
			("symbol", sym_str)
		);
		auto obj = json.get_object();
		auto obj_it = obj.find(sym_str);
		if (obj_it != obj.end()) {
			auto result = obj_it->value().as<gstio::chain_apis::read_only::get_currency_stats_result>();
			auto p = cache.emplace(make_pair(code, sym), result.max_supply.get_symbol());
			it = p.first;
		}
		else {
			GST_THROW(symbol_type_exception, "Symbol ${s} is not supported by token contract ${c}", ("s", sym_str)("c", code));
		}
	}
	auto expected_symbol = it->second;
	if (a.decimals() < expected_symbol.decimals()) {
		auto factor = expected_symbol.precision() / a.precision();
		auto a_old = a;
		a = asset(a.get_amount() * factor, expected_symbol);
	}
	else if (a.decimals() > expected_symbol.decimals()) {
		GST_THROW(symbol_type_exception, "Too many decimal digits in ${a}, only ${d} supported", ("a", a)("d", expected_symbol.decimals()));
	} // else precision matches
	return a;
}

inline asset to_asset(const string& s) {
	return to_asset(N(gstio.token), s);
}

struct set_account_permission_subcommand {
	string accountStr;
	string permissionStr;
	string authorityJsonOrFile;
	string parentStr;

	set_account_permission_subcommand(CLI::App* accountCmd) {
		auto permissions = accountCmd->add_subcommand("permission", localized("set parameters dealing with account permissions"));
		permissions->add_option("account", accountStr, localized("The account to set/delete a permission authority for"))->required();
		permissions->add_option("permission", permissionStr, localized("The permission name to set/delete an authority for"))->required();
		permissions->add_option("authority", authorityJsonOrFile, localized("[delete] NULL, [create/update] public key, JSON string, or filename defining the authority"))->required();
		permissions->add_option("parent", parentStr, localized("[create] The permission name of this parents permission (Defaults to: \"Active\")"));

		add_standard_transaction_options(permissions, "account@active");

		permissions->set_callback([this] {
			name account = name(accountStr);
			name permission = name(permissionStr);
			bool is_delete = boost::iequals(authorityJsonOrFile, "null");

			if (is_delete) {
				send_actions({ create_deleteauth(account, permission) });
			}
			else {
				authority auth = parse_json_authority_or_key(authorityJsonOrFile);

				name parent;
				if (parentStr.size() == 0 && permissionStr != "owner") {
					// see if we can auto-determine the proper parent
					const auto account_result = call(get_account_func, fc::mutable_variant_object("account_name", accountStr));
					const auto& existing_permissions = account_result.get_object()["permissions"].get_array();
					auto permissionPredicate = [this](const auto& perm) {
						return perm.is_object() &&
							perm.get_object().contains("perm_name") &&
							boost::equals(perm.get_object()["perm_name"].get_string(), permissionStr);
					};

					auto itr = boost::find_if(existing_permissions, permissionPredicate);
					if (itr != existing_permissions.end()) {
						parent = name((*itr).get_object()["parent"].get_string());
					}
					else {
						// if this is a new permission and there is no parent we default to "active"
						parent = name(config::active_name);

					}
				}
				else {
					parent = name(parentStr);
				}

				send_actions({ create_updateauth(account, permission, parent, auth) });
			}
		});
	}

};

struct set_action_permission_subcommand {
	string accountStr;
	string codeStr;
	string typeStr;
	string requirementStr;

	set_action_permission_subcommand(CLI::App* actionRoot) {
		auto permissions = actionRoot->add_subcommand("permission", localized("set parmaters dealing with account permissions"));
		permissions->add_option("account", accountStr, localized("The account to set/delete a permission authority for"))->required();
		permissions->add_option("code", codeStr, localized("The account that owns the code for the action"))->required();
		permissions->add_option("type", typeStr, localized("the type of the action"))->required();
		permissions->add_option("requirement", requirementStr, localized("[delete] NULL, [set/update] The permission name require for executing the given action"))->required();

		add_standard_transaction_options(permissions, "account@active");

		permissions->set_callback([this] {
			name account = name(accountStr);
			name code = name(codeStr);
			name type = name(typeStr);
			bool is_delete = boost::iequals(requirementStr, "null");

			if (is_delete) {
				send_actions({ create_unlinkauth(account, code, type) });
			}
			else {
				name requirement = name(requirementStr);
				send_actions({ create_linkauth(account, code, type, requirement) });
			}
		});
	}
};


bool local_port_used() {
	using namespace boost::asio;

	io_service ios;
	local::stream_protocol::endpoint endpoint(wallet_url.substr(strlen("unix://")));
	local::stream_protocol::socket socket(ios);
	boost::system::error_code ec;
	socket.connect(endpoint, ec);

	return !ec;
}

void try_local_port(uint32_t duration) {
	using namespace std::chrono;
	auto start_time = duration_cast<std::chrono::milliseconds>(system_clock::now().time_since_epoch()).count();
	while (!local_port_used()) {
		if (duration_cast<std::chrono::milliseconds>(system_clock::now().time_since_epoch()).count() - start_time > duration) {
			std::cerr << "Unable to connect to kgstd, if kgstd is running please kill the process and try again.\n";
			throw connection_exception(fc::log_messages{ FC_LOG_MESSAGE(error, "Unable to connect to kgstd") });
		}
	}
}

void ensure_kgstd_running(CLI::App* app) {
	if (no_auto_kgstd)
		return;
	// get, version, net do not require kgstd
	if (tx_skip_sign || app->got_subcommand("get") || app->got_subcommand("version") || app->got_subcommand("net"))
		return;
	if (app->get_subcommand("create")->got_subcommand("key")) // create key does not require wallet
		return;
	if (auto* subapp = app->get_subcommand("system")) {
		if (subapp->got_subcommand("listproducers") || subapp->got_subcommand("listbw") || subapp->got_subcommand("bidnameinfo")) // system list* do not require wallet
			return;
	}
	if (wallet_url != default_wallet_url)
		return;

	if (local_port_used())
		return;

	boost::filesystem::path binPath = boost::dll::program_location();
	binPath.remove_filename();
	// This extra check is necessary when running clgst like this: ./clgst ...
	if (binPath.filename_is_dot())
		binPath.remove_filename();
	binPath.append(key_store_executable_name); // if clgst and kgstd are in the same installation directory
	if (!boost::filesystem::exists(binPath)) {
		binPath.remove_filename().remove_filename().append("kgstd").append(key_store_executable_name);
	}

	if (boost::filesystem::exists(binPath)) {
		namespace bp = boost::process;
		binPath = boost::filesystem::canonical(binPath);

		vector<std::string> pargs;
		pargs.push_back("--http-server-address");
		pargs.push_back("");
		pargs.push_back("--https-server-address");
		pargs.push_back("");
		pargs.push_back("--unix-socket-path");
		pargs.push_back(string(key_store_executable_name) + ".sock");

		::boost::process::child kgst(binPath, pargs,
			bp::std_in.close(),
			bp::std_out > bp::null,
			bp::std_err > bp::null);
		if (kgst.running()) {
			std::cerr << binPath << " launched" << std::endl;
			kgst.detach();
			try_local_port(2000);
		}
		else {
			std::cerr << "No wallet service listening on " << wallet_url << ". Failed to launch " << binPath << std::endl;
		}
	}
	else {
		std::cerr << "No wallet service listening on "
			<< ". Cannot automatically start kgstd because kgstd was not found." << std::endl;
	}
}


CLI::callback_t obsoleted_option_host_port = [](CLI::results_t) {
	std::cerr << localized("Host and port options (-H, --wallet-host, etc.) have been replaced with -u/--url and --wallet-url\n"
		"Use for example -u http://localhost:8888 or --url https://example.invalid/\n");
	exit(1);
	return false;
};

struct register_producer_subcommand {
	string producer_str;
	string producer_key_str;
	string url;
	uint16_t loc = 0;

	register_producer_subcommand(CLI::App* actionRoot) {
		auto register_producer = actionRoot->add_subcommand("regproducer", localized("Register a new producer"));
		register_producer->add_option("account", producer_str, localized("The account to register as a producer"))->required();
		register_producer->add_option("producer_key", producer_key_str, localized("The producer's public key"))->required();
		register_producer->add_option("url", url, localized("url where info about producer can be found"), true);
		register_producer->add_option("location", loc, localized("relative location for purpose of nearest neighbor scheduling"), true);
		add_standard_transaction_options(register_producer);


		register_producer->set_callback([this] {
			public_key_type producer_key;
			try {
				producer_key = public_key_type(producer_key_str);
			} GST_RETHROW_EXCEPTIONS(public_key_type_exception, "Invalid producer public key: ${public_key}", ("public_key", producer_key_str))

				auto regprod_var = regproducer_variant(producer_str, producer_key, url, loc);
			send_actions({ create_action({permission_level{producer_str,config::active_name}}, config::system_account_name, N(regproducer), regprod_var) });
		});
	}
};

struct create_account_subcommand {
	string creator;
	string account_name;
	string owner_key_str;
	string active_key_str;
	string stake_net;
	string stake_cpu;
	uint32_t buy_ram_bytes_in_kbytes = 0;
	uint32_t buy_ram_bytes = 0;
	string buy_ram_gst;
	bool transfer;
	bool simple;

	create_account_subcommand(CLI::App* actionRoot, bool s) : simple(s) {
		auto createAccount = actionRoot->add_subcommand(
			(simple ? "account" : "newaccount"),
			(simple ? localized("Create a new account on the blockchain (assumes system contract does not restrict RAM usage)")
				: localized("Create a new account on the blockchain with initial resources"))
		);
		createAccount->add_option("creator", creator, localized("The name of the account creating the new account"))->required();
		createAccount->add_option("name", account_name, localized("The name of the new account"))->required();
		createAccount->add_option("OwnerKey", owner_key_str, localized("The owner public key for the new account"))->required();
		createAccount->add_option("ActiveKey", active_key_str, localized("The active public key for the new account"));

		if (!simple) {
			/*   createAccount->add_option("--stake-net", stake_net,
										 (localized("The amount of GST delegated for net bandwidth")))->required();
			   createAccount->add_option("--stake-cpu", stake_cpu,
										 (localized("The amount of GST delegated for CPU bandwidth")))->required();
			   createAccount->add_option("--buy-ram-kbytes", buy_ram_bytes_in_kbytes,
										 (localized("The amount of RAM bytes to purchase for the new account in kibibytes (KiB)")));
			   createAccount->add_option("--buy-ram-bytes", buy_ram_bytes,
										 (localized("The amount of RAM bytes to purchase for the new account in bytes")));
			   createAccount->add_option("--buy-ram", buy_ram_gst,
										 (localized("The amount of RAM bytes to purchase for the new account in GST"))); */
			createAccount->add_flag("--transfer", transfer,
				(localized("Transfer voting power and right to unstake GST to receiver")));
		}

		add_standard_transaction_options(createAccount);

		createAccount->set_callback([this] {
			if (!active_key_str.size())
				active_key_str = owner_key_str;
			public_key_type owner_key, active_key;
			try {
				owner_key = public_key_type(owner_key_str);
			} GST_RETHROW_EXCEPTIONS(public_key_type_exception, "Invalid owner public key: ${public_key}", ("public_key", owner_key_str));
			try {
				active_key = public_key_type(active_key_str);
			} GST_RETHROW_EXCEPTIONS(public_key_type_exception, "Invalid active public key: ${public_key}", ("public_key", active_key_str));
			auto create = create_newaccount(creator, account_name, owner_key, active_key);
			if (!simple) {
				//   GSTC_ASSERT( buy_ram_gst.size() || buy_ram_bytes_in_kbytes || buy_ram_bytes, "ERROR: One of --buy-ram, --buy-ram-kbytes or --buy-ram-bytes should have non-zero value" );
				GSTC_ASSERT(!buy_ram_bytes_in_kbytes || !buy_ram_bytes, "ERROR: --buy-ram-kbytes and --buy-ram-bytes cannot be set at the same time");
				action buymembytes = !buy_ram_gst.empty() ? create_buymem(creator, account_name, to_asset(buy_ram_gst))
					: create_buymembytes(creator, account_name, (buy_ram_bytes_in_kbytes) ? (buy_ram_bytes_in_kbytes * 1024) : buy_ram_bytes);
				// auto net = to_asset(stake_net);
				// auto cpu = to_asset(stake_cpu);
			 /*     if ( net.get_amount() != 0 || cpu.get_amount() != 0 ) {
					action delegate_account = create_delegate_account( creator, account_name, net, cpu, transfer);
					send_actions( { create, buymembytes, delegate_account } );
				 } else {
					send_actions( { create, buymembytes } );
				 }  */
				send_actions({ create });
			}
			else {
				send_actions({ create });
			}
		});
	}
};

struct unregister_producer_subcommand {
	string producer_str;

	unregister_producer_subcommand(CLI::App* actionRoot) {
		auto unregister_producer = actionRoot->add_subcommand("unregprod", localized("Unregister an existing producer"));
		unregister_producer->add_option("account", producer_str, localized("The account to unregister as a producer"))->required();
		add_standard_transaction_options(unregister_producer);

		unregister_producer->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("producer", producer_str);

			send_actions({ create_action({permission_level{producer_str,config::active_name}}, config::system_account_name, N(unregprod), act_payload) });
		});
	}
};

struct vote_producer_proxy_subcommand {
	string voter_str;
	string proxy_str;

	vote_producer_proxy_subcommand(CLI::App* actionRoot) {
		auto vote_proxy = actionRoot->add_subcommand("proxy", localized("Vote your stake through a proxy"));
		vote_proxy->add_option("voter", voter_str, localized("The voting account"))->required();
		vote_proxy->add_option("proxy", proxy_str, localized("The proxy account"))->required();
		add_standard_transaction_options(vote_proxy);

		vote_proxy->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("voter", voter_str)
				("proxy", proxy_str)
				("producers", std::vector<account_name>{});
			send_actions({ create_action({permission_level{voter_str,config::active_name}}, config::system_account_name, N(voteproducer), act_payload) });
		});
	}
};

struct vote_producers_subcommand {
	string voter_str;
	vector<gstio::name> producer_names;
	string vote_amount;

	vote_producers_subcommand(CLI::App* actionRoot) {
		auto vote_producers = actionRoot->add_subcommand("prods", localized("Vote for one or more producers"));
		vote_producers->add_option("voter", voter_str, localized("The voting account"))->required();
		//  vote_producers->add_option("vote_quantity", vote_amount, localized("The amount of GST to vote"))->required();
		vote_producers->add_option("producers", producer_names, localized("The account(s) to vote for. All options from this position and following will be treated as the producer list."))->required();
		add_standard_transaction_options(vote_producers);

		vote_producers->set_callback([this] {

			std::sort(producer_names.begin(), producer_names.end());

			fc::variant act_payload = fc::mutable_variant_object()
				("voter", voter_str)
				("proxy", "")
				//		 	  ("vote_quantity", to_asset(vote_amount))
				("producers", producer_names);
			//	   action delegate_account = create_delegate_account( voter_str, voter_str, to_asset(vote_amount), asset(0), true);
				//   send_actions( {  delegate_account } );
			send_actions({ create_action({permission_level{voter_str,config::active_name}}, config::system_account_name, N(voteproducer), act_payload) });
		});
	}
};

struct approve_producer_subcommand {
	gstio::name voter;
	gstio::name producer_name;

	approve_producer_subcommand(CLI::App* actionRoot) {
		auto approve_producer = actionRoot->add_subcommand("approve", localized("Add one producer to list of voted producers"));
		approve_producer->add_option("voter", voter, localized("The voting account"))->required();
		approve_producer->add_option("producer", producer_name, localized("The account to vote for"))->required();
		add_standard_transaction_options(approve_producer);

		approve_producer->set_callback([this] {
			auto result = call(get_table_func, fc::mutable_variant_object("json", true)
				("code", name(config::system_account_name).to_string())
				("scope", name(config::system_account_name).to_string())
				("table", "voters")
				("table_key", "owner")
				("lower_bound", voter.value)
				("limit", 1)
			);
			auto res = result.as<gstio::chain_apis::read_only::get_table_rows_result>();
			if (res.rows.empty() || res.rows[0]["owner"].as_string() != name(voter).to_string()) {
				std::cerr << "Voter info not found for account " << voter << std::endl;
				return;
			}
			GST_ASSERT(1 == res.rows.size(), multiple_voter_info, "More than one voter_info for account");
			auto prod_vars = res.rows[0]["producers"].get_array();
			vector<gstio::name> prods;
			for (auto& x : prod_vars) {
				prods.push_back(name(x.as_string()));
			}
			prods.push_back(producer_name);
			std::sort(prods.begin(), prods.end());
			auto it = std::unique(prods.begin(), prods.end());
			if (it != prods.end()) {
				std::cerr << "Producer \"" << producer_name << "\" is already on the list." << std::endl;
				return;
			}
			fc::variant act_payload = fc::mutable_variant_object()
				("voter", voter)
				("proxy", "")
				("producers", prods);
			send_actions({ create_action({permission_level{voter,config::active_name}}, config::system_account_name, N(voteproducer), act_payload) });
		});
	}
};

struct unapprove_producer_subcommand {
	gstio::name voter;
	gstio::name producer_name;

	unapprove_producer_subcommand(CLI::App* actionRoot) {
		auto approve_producer = actionRoot->add_subcommand("unapprove", localized("Remove one producer from list of voted producers"));
		approve_producer->add_option("voter", voter, localized("The voting account"))->required();
		approve_producer->add_option("producer", producer_name, localized("The account to remove from voted producers"))->required();
		add_standard_transaction_options(approve_producer);

		approve_producer->set_callback([this] {
			auto result = call(get_table_func, fc::mutable_variant_object("json", true)
				("code", name(config::system_account_name).to_string())
				("scope", name(config::system_account_name).to_string())
				("table", "voters")
				("table_key", "owner")
				("lower_bound", voter.value)
				("limit", 1)
			);
			auto res = result.as<gstio::chain_apis::read_only::get_table_rows_result>();
			if (res.rows.empty() || res.rows[0]["owner"].as_string() != name(voter).to_string()) {
				std::cerr << "Voter info not found for account " << voter << std::endl;
				return;
			}
			GST_ASSERT(1 == res.rows.size(), multiple_voter_info, "More than one voter_info for account");
			auto prod_vars = res.rows[0]["producers"].get_array();
			vector<gstio::name> prods;
			for (auto& x : prod_vars) {
				prods.push_back(name(x.as_string()));
			}
			auto it = std::remove(prods.begin(), prods.end(), producer_name);
			if (it == prods.end()) {
				std::cerr << "Cannot remove: producer \"" << producer_name << "\" is not on the list." << std::endl;
				return;
			}
			prods.erase(it, prods.end()); //should always delete only one element
			fc::variant act_payload = fc::mutable_variant_object()
				("voter", voter)
				("proxy", "")
				("producers", prods);
			send_actions({ create_action({permission_level{voter,config::active_name}}, config::system_account_name, N(voteproducer), act_payload) });
		});
	}
};

struct list_producers_subcommand {
	bool print_json = false;
	uint32_t limit = 50;
	std::string lower;

	list_producers_subcommand(CLI::App* actionRoot) {
		auto list_producers = actionRoot->add_subcommand("listproducers", localized("List producers"));
		list_producers->add_flag("--json,-j", print_json, localized("Output in JSON format"));
		list_producers->add_option("-l,--limit", limit, localized("The maximum number of rows to return"));
		list_producers->add_option("-L,--lower", lower, localized("lower bound value of key, defaults to first"));
		list_producers->set_callback([this] {
			auto rawResult = call(get_producers_func, fc::mutable_variant_object
			("json", true)("lower_bound", lower)("limit", limit));
			if (print_json) {
				std::cout << fc::json::to_pretty_string(rawResult) << std::endl;
				return;
			}
			auto result = rawResult.as<gstio::chain_apis::read_only::get_producers_result>();
			if (result.rows.empty()) {
				std::cout << "No producers found" << std::endl;
				return;
			}
			auto weight = result.total_producer_vote_weight;
			if (!weight)
				weight = 1;
			printf("%-13s %-57s %-59s %s\n", "Producer", "Producer key", "Url", "Scaled votes");
			for (auto& row : result.rows)
				printf("%-13.13s %-57.57s %-59.59s %1.4f\n",
					row["owner"].as_string().c_str(),
					row["producer_key"].as_string().c_str(),
					row["url"].as_string().c_str(),
					row["total_votes"].as_double() / weight);
			if (!result.more.empty())
				std::cout << "-L " << result.more << " for more" << std::endl;
		});
	}
};

struct get_schedule_subcommand {
	bool print_json = false;

	void print(const char* name, const fc::variant& schedule) {
		if (schedule.is_null()) {
			printf("%s schedule empty\n\n", name);
			return;
		}
		printf("%s schedule version %s\n", name, schedule["version"].as_string().c_str());
		printf("    %-13s %s\n", "Producer", "Producer key");
		printf("    %-13s %s\n", "=============", "==================");
		for (auto& row : schedule["producers"].get_array())
			printf("    %-13s %s\n", row["producer_name"].as_string().c_str(), row["block_signing_key"].as_string().c_str());
		printf("\n");
	}

	get_schedule_subcommand(CLI::App* actionRoot) {
		auto get_schedule = actionRoot->add_subcommand("schedule", localized("Retrieve the producer schedule"));
		get_schedule->add_flag("--json,-j", print_json, localized("Output in JSON format"));
		get_schedule->set_callback([this] {
			auto result = call(get_schedule_func, fc::mutable_variant_object());
			if (print_json) {
				std::cout << fc::json::to_pretty_string(result) << std::endl;
				return;
			}
			print("active", result["active"]);
			print("pending", result["pending"]);
			print("proposed", result["proposed"]);
		});
	}
};

struct get_transaction_id_subcommand {
	string trx_to_check;

	get_transaction_id_subcommand(CLI::App* actionRoot) {
		auto get_transaction_id = actionRoot->add_subcommand("transaction_id", localized("Get transaction id given transaction object"));
		get_transaction_id->add_option("transaction", trx_to_check, localized("The JSON string or filename defining the transaction which transaction id we want to retrieve"))->required();

		get_transaction_id->set_callback([&] {
			try {
				auto trx_var = json_from_file_or_string(trx_to_check);
				auto trx = trx_var.as<transaction>();
				std::cout << string(trx.id()) << std::endl;
			} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse transaction JSON '${data}'", ("data", trx_to_check))
		});
	}
};

struct delegate_bandwidth_subcommand {
	string from_str;
	string receiver_str;
	string stake_net_amount;
	//string stake_cpu_amount;
	string stake_storage_amount;
	string buy_ram_amount;
	uint32_t buy_ram_bytes = 0;
	bool transfer = false;

	delegate_bandwidth_subcommand(CLI::App* actionRoot) {
		auto delegate_bandwidth = actionRoot->add_subcommand("delegatebw", localized("Delegate bandwidth"));
		delegate_bandwidth->add_option("from", from_str, localized("The account to delegate bandwidth from"))->required();
		delegate_bandwidth->add_option("receiver", receiver_str, localized("The account to receive the delegated bandwidth"))->required();
		delegate_bandwidth->add_option("stake_net_quantity", stake_net_amount, localized("The amount of GST to stake for network bandwidth"))->required();
	//	delegate_bandwidth->add_option("stake_cpu_quantity", stake_cpu_amount, localized("The amount of GST to stake for CPU bandwidth"))->required();
		delegate_bandwidth->add_option("--buyram", buy_ram_amount, localized("The amount of GST to buyram"));
		delegate_bandwidth->add_option("--buy-ram-bytes", buy_ram_bytes, localized("The amount of RAM to buy in number of bytes"));
		delegate_bandwidth->add_flag("--transfer", transfer, localized("Transfer voting power and right to unstake GST to receiver"));
		add_standard_transaction_options(delegate_bandwidth);

		delegate_bandwidth->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("from", from_str)
				("receiver", receiver_str)
				("stake_net_quantity", to_asset(stake_net_amount))
		//		("stake_cpu_quantity", to_asset(stake_cpu_amount))
				("transfer", transfer);
			std::vector<chain::action> acts{ create_action({permission_level{from_str,config::active_name}}, config::system_account_name, N(delegatebw), act_payload) };
			GSTC_ASSERT(!(buy_ram_amount.size()) || !buy_ram_bytes, "ERROR: --buyram and --buy-ram-bytes cannot be set at the same time");
			if (buy_ram_amount.size()) {
				acts.push_back(create_buyram(from_str, receiver_str, to_asset(buy_ram_amount)));
			}
			else if (buy_ram_bytes) {
				acts.push_back(create_buyrambytes(from_str, receiver_str, buy_ram_bytes));
			}
			send_actions(std::move(acts));
		});
	}
};

struct undelegate_bandwidth_subcommand {
	string from_str;
	string receiver_str;
	string unstake_net_amount;
//	string unstake_cpu_amount;
	uint64_t unstake_storage_bytes;

	undelegate_bandwidth_subcommand(CLI::App* actionRoot) {
		auto undelegate_bandwidth = actionRoot->add_subcommand("undelegatebw", localized("Undelegate bandwidth"));
		undelegate_bandwidth->add_option("from", from_str, localized("The account undelegating bandwidth"))->required();
		undelegate_bandwidth->add_option("receiver", receiver_str, localized("The account to undelegate bandwidth from"))->required();
		undelegate_bandwidth->add_option("unstake_net_quantity", unstake_net_amount, localized("The amount of GST to undelegate for network bandwidth"))->required();
	//	undelegate_bandwidth->add_option("unstake_cpu_quantity", unstake_cpu_amount, localized("The amount of GST to undelegate for CPU bandwidth"))->required();
		add_standard_transaction_options(undelegate_bandwidth);

		undelegate_bandwidth->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("from", from_str)
				("receiver", receiver_str)
				("unstake_net_quantity", to_asset(unstake_net_amount));
//				("unstake_cpu_quantity", to_asset(unstake_cpu_amount));
			send_actions({ create_action({permission_level{from_str,config::active_name}}, config::system_account_name, N(undelegatebw), act_payload) });
		});
	}
};

struct bidname_subcommand {
	string bidder_str;
	string newname_str;
	string bid_amount;
	bidname_subcommand(CLI::App *actionRoot) {
		auto bidname = actionRoot->add_subcommand("bidname", localized("Name bidding"));
		bidname->add_option("bidder", bidder_str, localized("The bidding account"))->required();
		bidname->add_option("newname", newname_str, localized("The bidding name"))->required();
		bidname->add_option("bid", bid_amount, localized("The amount of GST to bid"))->required();
		add_standard_transaction_options(bidname);
		bidname->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("bidder", bidder_str)
				("newname", newname_str)
				("bid", to_asset(bid_amount));
			send_actions({ create_action({permission_level{bidder_str, config::active_name}}, config::system_account_name, N(bidname), act_payload) });
		});
	}
};

struct bidname_info_subcommand {
	bool print_json = false;
	string newname_str;
	bidname_info_subcommand(CLI::App* actionRoot) {
		auto list_producers = actionRoot->add_subcommand("bidnameinfo", localized("Get bidname info"));
		list_producers->add_flag("--json,-j", print_json, localized("Output in JSON format"));
		list_producers->add_option("newname", newname_str, localized("The bidding name"))->required();
		list_producers->set_callback([this] {
			auto rawResult = call(get_table_func, fc::mutable_variant_object("json", true)
				("code", "gstio")("scope", "gstio")("table", "namebids")
				("lower_bound", gstio::chain::string_to_name(newname_str.c_str()))("limit", 1));
			if (print_json) {
				std::cout << fc::json::to_pretty_string(rawResult) << std::endl;
				return;
			}
			auto result = rawResult.as<gstio::chain_apis::read_only::get_table_rows_result>();
			if (result.rows.empty()) {
				std::cout << "No bidname record found" << std::endl;
				return;
			}
			for (auto& row : result.rows) {
				fc::time_point time(fc::microseconds(row["last_bid_time"].as_uint64()));
				int64_t bid = row["high_bid"].as_int64();
				std::cout << std::left << std::setw(18) << "bidname:" << std::right << std::setw(24) << row["newname"].as_string() << "\n"
					<< std::left << std::setw(18) << "highest bidder:" << std::right << std::setw(24) << row["high_bidder"].as_string() << "\n"
					<< std::left << std::setw(18) << "highest bid:" << std::right << std::setw(24) << (bid > 0 ? bid : -bid) << "\n"
					<< std::left << std::setw(18) << "last bid time:" << std::right << std::setw(24) << ((std::string)time).c_str() << std::endl;
				if (bid < 0) std::cout << "This auction has already closed" << std::endl;
			}
		});
	}
};

struct list_bw_subcommand {
	gstio::name account;
	bool print_json = false;

	list_bw_subcommand(CLI::App* actionRoot) {
		auto list_bw = actionRoot->add_subcommand("listbw", localized("List delegated bandwidth"));
		list_bw->add_option("account", account, localized("The account delegated bandwidth"))->required();
		list_bw->add_flag("--json,-j", print_json, localized("Output in JSON format"));

		list_bw->set_callback([this] {
			//get entire table in scope of user account
			auto result = call(get_table_func, fc::mutable_variant_object("json", true)
				("code", name(config::system_account_name).to_string())
				("scope", account.to_string())
				("table", "delband")
			);
			if (!print_json) {
				auto res = result.as<gstio::chain_apis::read_only::get_table_rows_result>();
				if (!res.rows.empty()) {
					std::cout << std::setw(13) << std::left << "Receiver" << std::setw(21) << std::left << "Net bandwidth"
						<< std::setw(21) << std::left << "CPU bandwidth" << std::endl;
					for (auto& r : res.rows) {
						std::cout << std::setw(13) << std::left << r["to"].as_string()
							<< std::setw(21) << std::left << r["net_weight"].as_string()
							<< std::setw(21) << std::left << r["cpu_weight"].as_string()
							<< std::endl;
					}
				}
				else {
					std::cerr << "Delegated bandwidth not found" << std::endl;
				}
			}
			else {
				std::cout << fc::json::to_pretty_string(result) << std::endl;
			}
		});
	}
};
/*
struct buyram_subcommand {
   string from_str;
   string receiver_str;
   string amount;
   bool kbytes = false;
   bool bytes = false;

   buyram_subcommand(CLI::App* actionRoot) {
	  auto buyram = actionRoot->add_subcommand("buyram", localized("Buy RAM"));
	  buyram->add_option("payer", from_str, localized("The account paying for RAM"))->required();
	  buyram->add_option("receiver", receiver_str, localized("The account receiving bought RAM"))->required();
	  buyram->add_option("amount", amount, localized("The amount of GST to pay for RAM, or number of bytes/kibibytes of RAM if --bytes/--kbytes is set"))->required();
	  buyram->add_flag("--kbytes,-k", kbytes, localized("buyram in number of kibibytes (KiB)"));
	  buyram->add_flag("--bytes,-b", bytes, localized("buyram in number of bytes"));
	  add_standard_transaction_options(buyram);
	  buyram->set_callback([this] {
		 GSTC_ASSERT( !kbytes || !bytes, "ERROR: --kbytes and --bytes cannot be set at the same time" );
		 if (kbytes || bytes) {
			send_actions( { create_buyrambytes(from_str, receiver_str, fc::to_uint64(amount) * ((kbytes) ? 1024ull : 1ull)) } );
		 } else {
			send_actions( { create_buyram(from_str, receiver_str, to_asset(amount)) } );
		 }
	  });
   }
};

struct sellram_subcommand {
   string from_str;
   string receiver_str;
   uint64_t amount;

   sellram_subcommand(CLI::App* actionRoot) {
	  auto sellram = actionRoot->add_subcommand("sellram", localized("Sell RAM"));
	  sellram->add_option("account", receiver_str, localized("The account to receive GST for sold RAM"))->required();
	  sellram->add_option("bytes", amount, localized("Number of RAM bytes to sell"))->required();
	  add_standard_transaction_options(sellram);

	  sellram->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
			   ("account", receiver_str)
			   ("bytes", amount);
			send_actions({create_action({permission_level{receiver_str,config::active_name}}, config::system_account_name, N(sellram), act_payload)});
		 });
   }
}; */
struct voterewards_subcommand {
	string owner;

	voterewards_subcommand(CLI::App* actionRoot) {
		auto vote_rewards = actionRoot->add_subcommand("voterewards", localized("Claim voter rewards"));
		vote_rewards->add_option("owner", owner, localized("The account to vote rewards for"))->required();
		add_standard_transaction_options(vote_rewards);

		vote_rewards->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("owner", owner);
			send_actions({ create_action({permission_level{owner,config::active_name}}, config::system_account_name, N(voterewards), act_payload) });
		});
	}
};

struct claimrewards_subcommand {
	string owner;

	claimrewards_subcommand(CLI::App* actionRoot) {
		auto claim_rewards = actionRoot->add_subcommand("claimrewards", localized("Claim producer rewards"));
		claim_rewards->add_option("owner", owner, localized("The account to claim rewards for"))->required();
		add_standard_transaction_options(claim_rewards);

		claim_rewards->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("owner", owner);
			send_actions({ create_action({permission_level{owner,config::active_name}}, config::system_account_name, N(claimrewards), act_payload) });
		});
	}
};

//2019/03/12 以下
struct prodrewards_subcommand {
	string owner;

	prodrewards_subcommand(CLI::App* actionRoot) {
		auto prod_rewards = actionRoot->add_subcommand("prodrewards", localized("Claim producer rewards"));
		prod_rewards->add_option("owner", owner, localized("The account to claim rewards for"))->required();
		add_standard_transaction_options(prod_rewards);

		prod_rewards->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("owner", owner);
			send_actions({ create_action({permission_level{owner,config::active_name}}, config::system_account_name, N(prodrewards), act_payload) });
		});
	}
};
//2019/03/12 以下

//2019/03/11 以下
struct claimvrew_subcommand {
	string owner;

	claimvrew_subcommand(CLI::App* actionRoot) {
		auto claim_vrew = actionRoot->add_subcommand("claimvrew", localized("Claim producer rewards"));
		claim_vrew->add_option("owner", owner, localized("The account to claim rewards for"))->required();
		add_standard_transaction_options(claim_vrew);

		claim_vrew->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("owner", owner);
			send_actions({ create_action({permission_level{owner,config::active_name}}, config::system_account_name, N(claimvrew), act_payload) });
		});
	}
};
//2019/03/11 以上

struct regproxy_subcommand {
	string proxy;

	regproxy_subcommand(CLI::App* actionRoot) {
		auto register_proxy = actionRoot->add_subcommand("regproxy", localized("Register an account as a proxy (for voting)"));
		register_proxy->add_option("proxy", proxy, localized("The proxy account to register"))->required();
		add_standard_transaction_options(register_proxy);

		register_proxy->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("proxy", proxy)
				("isproxy", true);
			send_actions({ create_action({permission_level{proxy,config::active_name}}, config::system_account_name, N(regproxy), act_payload) });
		});
	}
};

struct unregproxy_subcommand {
	string proxy;

	unregproxy_subcommand(CLI::App* actionRoot) {
		auto unregister_proxy = actionRoot->add_subcommand("unregproxy", localized("Unregister an account as a proxy (for voting)"));
		unregister_proxy->add_option("proxy", proxy, localized("The proxy account to unregister"))->required();
		add_standard_transaction_options(unregister_proxy);

		unregister_proxy->set_callback([this] {
			fc::variant act_payload = fc::mutable_variant_object()
				("proxy", proxy)
				("isproxy", false);
			send_actions({ create_action({permission_level{proxy,config::active_name}}, config::system_account_name, N(regproxy), act_payload) });
		});
	}
};

struct canceldelay_subcommand {
	string canceling_account;
	string canceling_permission;
	string trx_id;

	canceldelay_subcommand(CLI::App* actionRoot) {
		auto cancel_delay = actionRoot->add_subcommand("canceldelay", localized("Cancel a delayed transaction"));
		cancel_delay->add_option("canceling_account", canceling_account, localized("Account from authorization on the original delayed transaction"))->required();
		cancel_delay->add_option("canceling_permission", canceling_permission, localized("Permission from authorization on the original delayed transaction"))->required();
		cancel_delay->add_option("trx_id", trx_id, localized("The transaction id of the original delayed transaction"))->required();
		add_standard_transaction_options(cancel_delay);

		cancel_delay->set_callback([this] {
			const auto canceling_auth = permission_level{ canceling_account, canceling_permission };
			fc::variant act_payload = fc::mutable_variant_object()
				("canceling_auth", canceling_auth)
				("trx_id", trx_id);
			send_actions({ create_action({canceling_auth}, config::system_account_name, N(canceldelay), act_payload) });
		});
	}
};

void get_account(const string& accountName, const string& coresym, bool json_format) {
	fc::variant json;
	if (coresym.empty()) {
		json = call(get_account_func, fc::mutable_variant_object("account_name", accountName));
	}
	else {
		json = call(get_account_func, fc::mutable_variant_object("account_name", accountName)("expected_core_symbol", symbol::from_string(coresym)));
	}

	auto res = json.as<gstio::chain_apis::read_only::get_account_results>();
	if (!json_format) {
		asset staked;
		asset unstaking;

		if (res.core_liquid_balance.valid()) {
			unstaking = asset(0, res.core_liquid_balance->get_symbol()); // Correct core symbol for unstaking asset.
			staked = asset(0, res.core_liquid_balance->get_symbol());    // Correct core symbol for staked asset.
		}

		std::cout << "created: " << string(res.created) << std::endl;

		if (res.privileged) std::cout << "privileged: true" << std::endl;

		constexpr size_t indent_size = 5;
		const string indent(indent_size, ' ');

		std::cout << "permissions: " << std::endl;
		unordered_map<name, vector<name>/*children*/> tree;
		vector<name> roots; //we don't have multiple roots, but we can easily handle them here, so let's do it just in case
		unordered_map<name, gstio::chain_apis::permission> cache;
		for (auto& perm : res.permissions) {
			if (perm.parent) {
				tree[perm.parent].push_back(perm.perm_name);
			}
			else {
				roots.push_back(perm.perm_name);
			}
			auto name = perm.perm_name; //keep copy before moving `perm`, since thirst argument of emplace can be evaluated first
			// looks a little crazy, but should be efficient
			cache.insert(std::make_pair(name, std::move(perm)));
		}
		std::function<void(account_name, int)> dfs_print = [&](account_name name, int depth) -> void {
			auto& p = cache.at(name);
			std::cout << indent << std::string(depth * 3, ' ') << name << ' ' << std::setw(5) << p.required_auth.threshold << ":    ";
			const char *sep = "";
			for (auto it = p.required_auth.keys.begin(); it != p.required_auth.keys.end(); ++it) {
				std::cout << sep << it->weight << ' ' << string(it->key);
				sep = ", ";
			}
			for (auto& acc : p.required_auth.accounts) {
				std::cout << sep << acc.weight << ' ' << string(acc.permission.actor) << '@' << string(acc.permission.permission);
				sep = ", ";
			}
			std::cout << std::endl;
			auto it = tree.find(name);
			if (it != tree.end()) {
				auto& children = it->second;
				sort(children.begin(), children.end());
				for (auto& n : children) {
					// we have a tree, not a graph, so no need to check for already visited nodes
					dfs_print(n, depth + 1);
				}
			} // else it's a leaf node
		};
		std::sort(roots.begin(), roots.end());
		for (auto r : roots) {
			dfs_print(r, 0);
		}

		auto to_pretty_net = [](int64_t nbytes, uint8_t width_for_units = 5) {
			if (nbytes == -1) {
				// special case. Treat it as unlimited
				return std::string("unlimited");
			}

			string unit = "bytes";
			double bytes = static_cast<double> (nbytes);
			if (bytes >= 1024 * 1024 * 1024 * 1024ll) {
				unit = "TiB";
				bytes /= 1024 * 1024 * 1024 * 1024ll;
			}
			else if (bytes >= 1024 * 1024 * 1024) {
				unit = "GiB";
				bytes /= 1024 * 1024 * 1024;
			}
			else if (bytes >= 1024 * 1024) {
				unit = "MiB";
				bytes /= 1024 * 1024;
			}
			else if (bytes >= 1024) {
				unit = "KiB";
				bytes /= 1024;
			}
			std::stringstream ss;
			ss << setprecision(4);
			ss << bytes << " ";
			if (width_for_units > 0)
				ss << std::left << setw(width_for_units);
			ss << unit;
			return ss.str();
		};



		//       std::cout << "memory: " << std::endl
		//                 << indent << "quota: " << std::setw(15) << to_pretty_net(res.ram_quota) << "  used: " << std::setw(15) << to_pretty_net(res.ram_usage) << std::endl << std::endl;

		//       std::cout << "net bandwidth: " << std::endl;
		// if (res.total_resources.is_object()) {

		// 	auto net_total = to_asset(res.total_resources.get_object()["net_weight"].as_string());
		// 	if (net_total.get_symbol() != unstaking.get_symbol()) {
		// 		// Core symbol of nodgst responding to the request is different than core symbol built into clgst
		// 		unstaking = asset(0, net_total.get_symbol()); // Correct core symbol for unstaking asset.
		// 		staked = asset(0, net_total.get_symbol()); // Correct core symbol for staked asset.
		// 	}

		// 	if (res.self_delegated_bandwidth.is_object()) {
		// 		asset net_own = asset::from_string(res.self_delegated_bandwidth.get_object()["net_weight"].as_string());
		// 		staked = net_own;


		// 		auto net_others = net_total - net_own;

		// 		        std::cout << indent << "staked:" << std::setw(20) << net_own
		// 							<< std::string(11, ' ') << "(total stake delegated from account to self)" << std::endl
		// 							<< indent << "delegated:" << std::setw(17) << net_others
		// 							<< std::string(11, ' ') << "(total staked delegated to account from others)" << std::endl;  */
		// 	}
		// 	else {
		// 		auto net_others = net_total;
		// 		         std::cout << indent << "delegated:" << std::setw(17) << net_others
		// 							<< std::string(11, ' ') << "(total staked delegated to account from others)" << std::endl; */
		// 	}
		// }


		auto to_pretty_time = [](int64_t nmicro, uint8_t width_for_units = 5) {
			if (nmicro == -1) {
				// special case. Treat it as unlimited
			//    return std::string("unlimited");
			}
			string unit = "us";
			double micro = static_cast<double>(nmicro);

			if (micro > 1000000 * 60 * 60ll) {
				micro /= 1000000 * 60 * 60ll;
				unit = "hr";
			}
			else if (micro > 1000000 * 60) {
				micro /= 1000000 * 60;
				unit = "min";
			}
			else if (micro > 1000000) {
				micro /= 1000000;
				unit = "sec";
			}
			else if (micro > 1000) {
				micro /= 1000;
				unit = "ms";
			}
			std::stringstream ss;
			ss << setprecision(4);
			ss << micro << " ";
			if (width_for_units > 0)
				ss << std::left << setw(width_for_units);
			ss << unit;
			return ss.str();
		};


		//     std::cout << std::fixed << setprecision(3);
		//     std::cout << indent << std::left << std::setw(11) << "used:"      << std::right << std::setw(18) << to_pretty_net( res.net_limit.used ) << "\n";
		//     std::cout << indent << std::left << std::setw(11) << "available:" << std::right << std::setw(18) << to_pretty_net( res.net_limit.available ) << "\n";
		//     std::cout << indent << std::left << std::setw(11) << "limit:"     << std::right << std::setw(18) << to_pretty_net( res.net_limit.max ) << "\n";
		//     std::cout << std::endl;

		//     std::cout << "cpu bandwidth:" << std::endl;

		// if (res.total_resources.is_object()) {
		// 	auto cpu_total = to_asset(res.total_resources.get_object()["cpu_weight"].as_string());

		// 	if (res.self_delegated_bandwidth.is_object()) {
		// 		asset cpu_own = asset::from_string(res.self_delegated_bandwidth.get_object()["cpu_weight"].as_string());
		// 		staked += cpu_own;

		// 		auto cpu_others = cpu_total - cpu_own;

		// 		          std::cout << indent << "staked:" << std::setw(20) << cpu_own
		// 		                    << std::string(11, ' ') << "(total stake delegated from account to self)" << std::endl
		// 		                    << indent << "delegated:" << std::setw(17) << cpu_others
		// 		                    << std::string(11, ' ') << "(total staked delegated to account from others)" << std::endl;
		// 	}
		// 	else {
		// 		auto cpu_others = cpu_total;
		// 		       std::cout << indent << "delegated:" << std::setw(17) << cpu_others
		// 		                 << std::string(11, ' ') << "(total staked delegated to account from others)" << std::endl;
		// 	}
		// }


		//   std::cout << std::fixed << setprecision(3);
		//   std::cout << indent << std::left << std::setw(11) << "used:"      << std::right << std::setw(18) << to_pretty_time( res.cpu_limit.used ) << "\n";
		//   std::cout << indent << std::left << std::setw(11) << "available:" << std::right << std::setw(18) << to_pretty_time( res.cpu_limit.available ) << "\n";
		//   std::cout << indent << std::left << std::setw(11) << "limit:"     << std::right << std::setw(18) << to_pretty_time( res.cpu_limit.max ) << "\n";
		//   std::cout << std::endl;

		// if (res.refund_request.is_object()) {
		// 	auto obj = res.refund_request.get_object();
		// 	auto request_time = fc::time_point_sec::from_iso_string(obj["request_time"].as_string());
		// 	fc::time_point refund_time = request_time + fc::days(3);
		// 	auto now = res.head_block_time;
		// 	asset net = asset::from_string(obj["net_amount"].as_string());
		// 	asset cpu = asset::from_string(obj["cpu_amount"].as_string());
		// 	unstaking = net + cpu;


		// 	if (unstaking > asset(0, unstaking.get_symbol())) {
		// 		       std::cout << std::fixed << setprecision(3);
		// 	           std::cout << "unstaking tokens:" << std::endl;
		// 	           std::cout << indent << std::left << std::setw(25) << "time of unstake request:" << std::right << std::setw(20) << string(request_time);
		// 		if (now >= refund_time) {
		// 			          std::cout << " (available to claim now with 'gstio::refund' action)\n";
		// 		}
		// 		else {
		// 			         std::cout << " (funds will be available in " << to_pretty_time( (refund_time - now).count(), 0 ) << ")\n";
		// 		}
		// 		     std::cout << indent << std::left << std::setw(25) << "from net bandwidth:" << std::right << std::setw(18) << net << std::endl;
		// 		     std::cout << indent << std::left << std::setw(25) << "from cpu bandwidth:" << std::right << std::setw(18) << cpu << std::endl;
		// 		     std::cout << indent << std::left << std::setw(25) << "total:" << std::right << std::setw(18) << unstaking << std::endl;
		// 		     std::cout << std::endl;
		// 	}
		// }
		
		//2019/03/15 以下
		if (res.core_liquid_balance.valid()) {
			std::cout << res.core_liquid_balance->get_symbol().name() << " balances: " << std::endl;
			std::cout << indent << std::left << std::setw(11)
				<< "liquid:" << std::right << std::setw(18) << *res.core_liquid_balance << std::endl;
			// std::cout << indent << std::left << std::setw(11)
			// 	<< "staked:" << std::right << std::setw(18) << staked << std::endl;
			// std::cout << indent << std::left << std::setw(11)
			// 	<< "unstaking:" << std::right << std::setw(18) << unstaking << std::endl;
			// std::cout << indent << std::left << std::setw(11) << "total:" << std::right << std::setw(18) << (*res.core_liquid_balance + staked + unstaking) << std::endl;

			// std::cout << std::endl;
		}

		if (res.self_delegated_bandwidth.is_object()) {
			auto obj = res.self_delegated_bandwidth.get_object();
			asset staked = asset::from_string(obj["net_weight"].as_string());
			std::cout << indent << std::left << std::setw(11)
				<< "staked:" << std::right << std::setw(18) << staked << std::endl;

		}

		if(res.refund_request.is_object()){
			auto obj = res.refund_request.get_object();
			auto request_time = fc::time_point_sec::from_iso_string(obj["request_time"].as_string());
			fc::time_point refund_time = request_time + fc::days(3);
			auto now = res.head_block_time;
			asset unstaking = asset::from_string(obj["net_amount"].as_string());
			std::cout << indent << std::left << std::setw(11)
				<< "unstaking:" << std::right << std::setw(18) << unstaking << " (funds will be available in " 
				<< to_pretty_time( (refund_time - now).count(), 0 ) << ")" << std::endl;
		}

		if (res.total_resources.is_object()) {
			auto obj = res.total_resources.get_object();
			asset votereward = asset::from_string(obj["votereward"].as_string());
			asset prodreward = asset::from_string(obj["prodreward"].as_string());
			std::cout << indent << std::left << std::setw(11)
				<< "votereward:" << std::right << std::setw(18) << votereward << std::endl;
			std::cout << indent << std::left << std::setw(11)
				<< "prodreward:" << std::right << std::setw(18) << prodreward << std::endl;
			
			std::cout << std::endl;
		}
		//2019/03/15 以上


		if (res.voter_info.is_object()) {
			auto& obj = res.voter_info.get_object();
			string proxy = obj["proxy"].as_string();
			if (proxy.empty()) {
				auto& prods = obj["producers"].get_array();
				std::cout << "producers:";
				if (!prods.empty()) {
					for (int i = 0; i < prods.size(); ++i) {
						if (i % 3 == 0) {
							std::cout << std::endl << indent;
						}
						std::cout << std::setw(16) << std::left << prods[i].as_string();
					}
					std::cout << std::endl;
				}
				else {
					std::cout << indent << "<not voted>" << std::endl;
				}
			}
			else {
				std::cout << "proxy:" << indent << proxy << std::endl;
			}
		}
		std::cout << std::endl;
	}
	else {
		std::cout << fc::json::to_pretty_string(json) << std::endl;
	}
}

CLI::callback_t header_opt_callback = [](CLI::results_t res) {
	vector<string>::iterator itr;

	for (itr = res.begin(); itr != res.end(); itr++) {
		headers.push_back(*itr);
	}

	return true;
};

int main(int argc, char** argv) {
	setlocale(LC_ALL, "");
	bindtextdomain(locale_domain, locale_path);
	textdomain(locale_domain);
	context = gstio::client::http::create_http_context();
	wallet_url = default_wallet_url;

	CLI::App app{ "Command Line Interface to GSTIO Client" };
	app.require_subcommand();
	app.add_option("-H,--host", obsoleted_option_host_port, localized("the host where nodgst is running"))->group("hidden");
	app.add_option("-p,--port", obsoleted_option_host_port, localized("the port where nodgst is running"))->group("hidden");
	app.add_option("--wallet-host", obsoleted_option_host_port, localized("the host where kgstd is running"))->group("hidden");
	app.add_option("--wallet-port", obsoleted_option_host_port, localized("the port where kgstd is running"))->group("hidden");

	app.add_option("-u,--url", url, localized("the http/https URL where nodgst is running"), true);
	app.add_option("--wallet-url", wallet_url, localized("the http/https URL where kgstd is running"), true);

	app.add_option("-r,--header", header_opt_callback, localized("pass specific HTTP header; repeat this option to pass multiple headers"));
	app.add_flag("-n,--no-verify", no_verify, localized("don't verify peer certificate when using HTTPS"));
	app.add_flag("--no-auto-kgstd", no_auto_kgstd, localized("don't automatically launch a kgstd if one is not currently running"));
	app.set_callback([&app] { ensure_kgstd_running(&app); });

	bool verbose_errors = false;
	app.add_flag("-v,--verbose", verbose_errors, localized("output verbose actions on error"));
	app.add_flag("--print-request", print_request, localized("print HTTP request to STDERR"));
	app.add_flag("--print-response", print_response, localized("print HTTP response to STDERR"));

	auto version = app.add_subcommand("version", localized("Retrieve version information"), false);
	version->require_subcommand();

	version->add_subcommand("client", localized("Retrieve version information of the client"))->set_callback([] {
		std::cout << localized("Build version: ${ver}", ("ver", gstio::client::config::version_str)) << std::endl;
	});

	// Create subcommand
	auto create = app.add_subcommand("create", localized("Create various items, on and off the blockchain"), false);
	create->require_subcommand();

	bool r1 = false;
	string key_file;
	bool print_console = false;
	// create key
	auto create_key = create->add_subcommand("key", localized("Create a new keypair and print the public and private keys"))->set_callback([&r1, &key_file, &print_console]() {
		if (key_file.empty() && !print_console) {
			std::cerr << "ERROR: Either indicate a file using \"--file\" or pass \"--to-console\"" << std::endl;
			return;
		}

		auto pk = r1 ? private_key_type::generate_r1() : private_key_type::generate();
		auto privs = string(pk);
		auto pubs = string(pk.get_public_key());
		if (print_console) {
			std::cout << localized("Private key: ${key}", ("key", privs)) << std::endl;
			std::cout << localized("Public key: ${key}", ("key", pubs)) << std::endl;
		}
		else {
			std::cerr << localized("saving keys to ${filename}", ("filename", key_file)) << std::endl;
			std::ofstream out(key_file.c_str());
			out << localized("Private key: ${key}", ("key", privs)) << std::endl;
			out << localized("Public key: ${key}", ("key", pubs)) << std::endl;
		}
	});
	create_key->add_flag("--r1", r1, "Generate a key using the R1 curve (iPhone), instead of the K1 curve (Bitcoin)");
	create_key->add_option("-f,--file", key_file, localized("Name of file to write private/public key output to. (Must be set, unless \"--to-console\" is passed"));
	create_key->add_flag("--to-console", print_console, localized("Print private/public keys to console."));

	// create account
	auto createAccount = create_account_subcommand(create, true /*simple*/);

	// convert subcommand
	auto convert = app.add_subcommand("convert", localized("Pack and unpack transactions"), false); // TODO also add converting action args based on abi from here ?
	convert->require_subcommand();

	// pack transaction
	string plain_signed_transaction_json;
	bool pack_action_data_flag = false;
	auto pack_transaction = convert->add_subcommand("pack_transaction", localized("From plain signed json to packed form"));
	pack_transaction->add_option("transaction", plain_signed_transaction_json, localized("The plain signed json (string)"))->required();
	pack_transaction->add_flag("--pack-action-data", pack_action_data_flag, localized("Pack all action data within transaction, needs interaction with nodgst"));
	pack_transaction->set_callback([&] {
		fc::variant trx_var;
		try {
			trx_var = json_from_file_or_string(plain_signed_transaction_json);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse plain transaction JSON '${data}'", ("data", plain_signed_transaction_json))
			if (pack_action_data_flag) {
				signed_transaction trx;
				abi_serializer::from_variant(trx_var, trx, abi_serializer_resolver, abi_serializer_max_time);
				std::cout << fc::json::to_pretty_string(packed_transaction(trx, packed_transaction::none)) << std::endl;
			}
			else {
				try {
					signed_transaction trx = trx_var.as<signed_transaction>();
					std::cout << fc::json::to_pretty_string(fc::variant(packed_transaction(trx, packed_transaction::none))) << std::endl;
				} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to convert transaction, --pack-action-data likely needed")
			}
	});

	// unpack transaction
	string packed_transaction_json;
	bool unpack_action_data_flag = false;
	auto unpack_transaction = convert->add_subcommand("unpack_transaction", localized("From packed to plain signed json form"));
	unpack_transaction->add_option("transaction", packed_transaction_json, localized("The packed transaction json (string containing packed_trx and optionally compression fields)"))->required();
	unpack_transaction->add_flag("--unpack-action-data", unpack_action_data_flag, localized("Unpack all action data within transaction, needs interaction with nodgst"));
	unpack_transaction->set_callback([&] {
		fc::variant packed_trx_var;
		packed_transaction packed_trx;
		try {
			packed_trx_var = json_from_file_or_string(packed_transaction_json);
			fc::from_variant<packed_transaction>(packed_trx_var, packed_trx);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse packed transaction JSON '${data}'", ("data", packed_transaction_json))
			signed_transaction strx = packed_trx.get_signed_transaction();
		fc::variant trx_var;
		if (unpack_action_data_flag) {
			abi_serializer::to_variant(strx, trx_var, abi_serializer_resolver, abi_serializer_max_time);
		}
		else {
			trx_var = strx;
		}
		std::cout << fc::json::to_pretty_string(trx_var) << std::endl;
	});

	// pack action data
	string unpacked_action_data_account_string;
	string unpacked_action_data_name_string;
	string unpacked_action_data_string;
	auto pack_action_data = convert->add_subcommand("pack_action_data", localized("From json action data to packed form"));
	pack_action_data->add_option("account", unpacked_action_data_account_string, localized("The name of the account that hosts the contract"))->required();
	pack_action_data->add_option("name", unpacked_action_data_name_string, localized("The name of the function that's called by this action"))->required();
	pack_action_data->add_option("unpacked_action_data", unpacked_action_data_string, localized("The action data expressed as json"))->required();
	pack_action_data->set_callback([&] {
		fc::variant unpacked_action_data_json;
		try {
			unpacked_action_data_json = json_from_file_or_string(unpacked_action_data_string);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse unpacked action data JSON")
			bytes packed_action_data_string = variant_to_bin(unpacked_action_data_account_string, unpacked_action_data_name_string, unpacked_action_data_json);
		std::cout << fc::to_hex(packed_action_data_string.data(), packed_action_data_string.size()) << std::endl;
	});

	// unpack action data
	string packed_action_data_account_string;
	string packed_action_data_name_string;
	string packed_action_data_string;
	auto unpack_action_data = convert->add_subcommand("unpack_action_data", localized("From packed to json action data form"));
	unpack_action_data->add_option("account", packed_action_data_account_string, localized("The name of the account that hosts the contract"))->required();
	unpack_action_data->add_option("name", packed_action_data_name_string, localized("The name of the function that's called by this action"))->required();
	unpack_action_data->add_option("packed_action_data", packed_action_data_string, localized("The action data expressed as packed hex string"))->required();
	unpack_action_data->set_callback([&] {
		GST_ASSERT(packed_action_data_string.size() >= 2, transaction_type_exception, "No packed_action_data found");
		vector<char> packed_action_data_blob(packed_action_data_string.size() / 2);
		fc::from_hex(packed_action_data_string, packed_action_data_blob.data(), packed_action_data_blob.size());
		fc::variant unpacked_action_data_json = bin_to_variant(packed_action_data_account_string, packed_action_data_name_string, packed_action_data_blob);
		std::cout << fc::json::to_pretty_string(unpacked_action_data_json) << std::endl;
	});

	// Get subcommand
	auto get = app.add_subcommand("get", localized("Retrieve various items and information from the blockchain"), false);
	get->require_subcommand();

	// get info
	get->add_subcommand("info", localized("Get current blockchain information"))->set_callback([] {
		std::cout << fc::json::to_pretty_string(get_info()) << std::endl;
	});

	// get block
	string blockArg;
	bool get_bhs = false;
	auto getBlock = get->add_subcommand("block", localized("Retrieve a full block from the blockchain"), false);
	getBlock->add_option("block", blockArg, localized("The number or ID of the block to retrieve"))->required();
	getBlock->add_flag("--header-state", get_bhs, localized("Get block header state from fork database instead"));
	getBlock->set_callback([&blockArg, &get_bhs] {
		auto arg = fc::mutable_variant_object("block_num_or_id", blockArg);
		if (get_bhs) {
			std::cout << fc::json::to_pretty_string(call(get_block_header_state_func, arg)) << std::endl;
		}
		else {
			std::cout << fc::json::to_pretty_string(call(get_block_func, arg)) << std::endl;
		}
	});

	// get account
	string accountName;
	string coresym;
	bool print_json;
	auto getAccount = get->add_subcommand("account", localized("Retrieve an account from the blockchain"), false);
	getAccount->add_option("name", accountName, localized("The name of the account to retrieve"))->required();
	getAccount->add_option("core-symbol", coresym, localized("The expected core symbol of the chain you are querying"));
	getAccount->add_flag("--json,-j", print_json, localized("Output in JSON format"));
	getAccount->set_callback([&]() { get_account(accountName, coresym, print_json); });

	// get code
	string codeFilename;
	string abiFilename;
	bool code_as_wasm = false;
	auto getCode = get->add_subcommand("code", localized("Retrieve the code and ABI for an account"), false);
	getCode->add_option("name", accountName, localized("The name of the account whose code should be retrieved"))->required();
	getCode->add_option("-c,--code", codeFilename, localized("The name of the file to save the contract .wast/wasm to"));
	getCode->add_option("-a,--abi", abiFilename, localized("The name of the file to save the contract .abi to"));
	getCode->add_flag("--wasm", code_as_wasm, localized("Save contract as wasm"));
	getCode->set_callback([&] {
		string code_hash, wasm, wast, abi;
		try {
			const auto result = call(get_raw_code_and_abi_func, fc::mutable_variant_object("account_name", accountName));
			const std::vector<char> wasm_v = result["wasm"].as_blob().data;
			const std::vector<char> abi_v = result["abi"].as_blob().data;

			fc::sha256 hash;
			if (wasm_v.size())
				hash = fc::sha256::hash(wasm_v.data(), wasm_v.size());
			code_hash = (string)hash;

			wasm = string(wasm_v.begin(), wasm_v.end());
			if (!code_as_wasm && wasm_v.size())
				wast = wasm_to_wast((const uint8_t*)wasm_v.data(), wasm_v.size(), false);

			abi_def abi_d;
			if (abi_serializer::to_abi(abi_v, abi_d))
				abi = fc::json::to_pretty_string(abi_d);
		}
		catch (chain::missing_chain_api_plugin_exception&) {
			//see if this is an old nodgst that doesn't support get_raw_code_and_abi
			const auto old_result = call(get_code_func, fc::mutable_variant_object("account_name", accountName)("code_as_wasm", code_as_wasm));
			code_hash = old_result["code_hash"].as_string();
			if (code_as_wasm) {
				wasm = old_result["wasm"].as_string();
				std::cout << localized("Warning: communicating to older nodgst which returns malformed binary wasm") << std::endl;
			}
			else
				wast = old_result["wast"].as_string();
			abi = fc::json::to_pretty_string(old_result["abi"]);
		}

		std::cout << localized("code hash: ${code_hash}", ("code_hash", code_hash)) << std::endl;

		if (codeFilename.size()) {
			std::cout << localized("saving ${type} to ${codeFilename}", ("type", (code_as_wasm ? "wasm" : "wast"))("codeFilename", codeFilename)) << std::endl;

			std::ofstream out(codeFilename.c_str());
			if (code_as_wasm)
				out << wasm;
			else
				out << wast;
		}
		if (abiFilename.size()) {
			std::cout << localized("saving abi to ${abiFilename}", ("abiFilename", abiFilename)) << std::endl;
			std::ofstream abiout(abiFilename.c_str());
			abiout << abi;
		}
	});

	// get abi
	string filename;
	auto getAbi = get->add_subcommand("abi", localized("Retrieve the ABI for an account"), false);
	getAbi->add_option("name", accountName, localized("The name of the account whose abi should be retrieved"))->required();
	getAbi->add_option("-f,--file", filename, localized("The name of the file to save the contract .abi to instead of writing to console"));
	getAbi->set_callback([&] {
		auto result = call(get_abi_func, fc::mutable_variant_object("account_name", accountName));

		auto abi = fc::json::to_pretty_string(result["abi"]);
		if (filename.size()) {
			std::cerr << localized("saving abi to ${filename}", ("filename", filename)) << std::endl;
			std::ofstream abiout(filename.c_str());
			abiout << abi;
		}
		else {
			std::cout << abi << "\n";
		}
	});

	// get table
	string scope;
	string code;
	string table;
	string lower;
	string upper;
	string table_key;
	string key_type;
	string encode_type{ "dec" };
	bool binary = false;
	uint32_t limit = 10;
	string index_position;
	auto getTable = get->add_subcommand("table", localized("Retrieve the contents of a database table"), false);
	getTable->add_option("account", code, localized("The account who owns the table"))->required();
	getTable->add_option("scope", scope, localized("The scope within the contract in which the table is found"))->required();
	getTable->add_option("table", table, localized("The name of the table as specified by the contract abi"))->required();
	getTable->add_option("-b,--binary", binary, localized("Return the value as BINARY rather than using abi to interpret as JSON"));
	getTable->add_option("-l,--limit", limit, localized("The maximum number of rows to return"));
	getTable->add_option("-k,--key", table_key, localized("Deprecated"));
	getTable->add_option("-L,--lower", lower, localized("JSON representation of lower bound value of key, defaults to first"));
	getTable->add_option("-U,--upper", upper, localized("JSON representation of upper bound value of key, defaults to last"));
	getTable->add_option("--index", index_position,
		localized("Index number, 1 - primary (first), 2 - secondary index (in order defined by multi_index), 3 - third index, etc.\n"
			"\t\t\t\tNumber or name of index can be specified, e.g. 'secondary' or '2'."));
	getTable->add_option("--key-type", key_type,
		localized("The key type of --index, primary only supports (i64), all others support (i64, i128, i256, float64, float128, ripemd160, sha256).\n"
			"\t\t\t\tSpecial type 'name' indicates an account name."));
	getTable->add_option("--encode-type", encode_type,
		localized("The encoding type of key_type (i64 , i128 , float64, float128) only support decimal encoding e.g. 'dec'"
			"i256 - supports both 'dec' and 'hex', ripemd160 and sha256 is 'hex' only\n"));


	getTable->set_callback([&] {
		auto result = call(get_table_func, fc::mutable_variant_object("json", !binary)
			("code", code)
			("scope", scope)
			("table", table)
			("table_key", table_key) // not used
			("lower_bound", lower)
			("upper_bound", upper)
			("limit", limit)
			("key_type", key_type)
			("index_position", index_position)
			("encode_type", encode_type)
		);

		std::cout << fc::json::to_pretty_string(result)
			<< std::endl;
	});

	auto getScope = get->add_subcommand("scope", localized("Retrieve a list of scopes and tables owned by a contract"), false);
	getScope->add_option("contract", code, localized("The contract who owns the table"))->required();
	getScope->add_option("-t,--table", table, localized("The name of the table as filter"));
	getScope->add_option("-l,--limit", limit, localized("The maximum number of rows to return"));
	getScope->add_option("-L,--lower", lower, localized("lower bound of scope"));
	getScope->add_option("-U,--upper", upper, localized("upper bound of scope"));
	getScope->set_callback([&] {
		auto result = call(get_table_by_scope_func, fc::mutable_variant_object("code", code)
			("table", table)
			("lower_bound", lower)
			("upper_bound", upper)
			("limit", limit)
		);
		std::cout << fc::json::to_pretty_string(result)
			<< std::endl;
	});

	// currency accessors
	// get currency balance
	string symbol;
	auto get_currency = get->add_subcommand("currency", localized("Retrieve information related to standard currencies"), true);
	get_currency->require_subcommand();
	auto get_balance = get_currency->add_subcommand("balance", localized("Retrieve the balance of an account for a given currency"), false);
	get_balance->add_option("contract", code, localized("The contract that operates the currency"))->required();
	get_balance->add_option("account", accountName, localized("The account to query balances for"))->required();
	get_balance->add_option("symbol", symbol, localized("The symbol for the currency if the contract operates multiple currencies"));
	get_balance->set_callback([&] {
		auto result = call(get_currency_balance_func, fc::mutable_variant_object
		("account", accountName)
			("code", code)
			("symbol", symbol.empty() ? fc::variant() : symbol)
		);

		const auto& rows = result.get_array();
		for (const auto& r : rows) {
			std::cout << r.as_string()
				<< std::endl;
		}
	});

	auto get_currency_stats = get_currency->add_subcommand("stats", localized("Retrieve the stats of for a given currency"), false);
	get_currency_stats->add_option("contract", code, localized("The contract that operates the currency"))->required();
	get_currency_stats->add_option("symbol", symbol, localized("The symbol for the currency if the contract operates multiple currencies"))->required();
	get_currency_stats->set_callback([&] {
		auto result = call(get_currency_stats_func, fc::mutable_variant_object("json", false)
			("code", code)
			("symbol", symbol)
		);

		std::cout << fc::json::to_pretty_string(result)
			<< std::endl;
	});

	// get accounts
	string public_key_str;
	auto getAccounts = get->add_subcommand("accounts", localized("Retrieve accounts associated with a public key"), false);
	getAccounts->add_option("public_key", public_key_str, localized("The public key to retrieve accounts for"))->required();
	getAccounts->set_callback([&] {
		public_key_type public_key;
		try {
			public_key = public_key_type(public_key_str);
		} GST_RETHROW_EXCEPTIONS(public_key_type_exception, "Invalid public key: ${public_key}", ("public_key", public_key_str))
			auto arg = fc::mutable_variant_object("public_key", public_key);
		std::cout << fc::json::to_pretty_string(call(get_key_accounts_func, arg)) << std::endl;
	});


	// get servants
	string controllingAccount;
	auto getServants = get->add_subcommand("servants", localized("Retrieve accounts which are servants of a given account "), false);
	getServants->add_option("account", controllingAccount, localized("The name of the controlling account"))->required();
	getServants->set_callback([&] {
		auto arg = fc::mutable_variant_object("controlling_account", controllingAccount);
		std::cout << fc::json::to_pretty_string(call(get_controlled_accounts_func, arg)) << std::endl;
	});

	// get transaction
	string transaction_id_str;
	uint32_t block_num_hint = 0;
	auto getTransaction = get->add_subcommand("transaction", localized("Retrieve a transaction from the blockchain"), false);
	getTransaction->add_option("id", transaction_id_str, localized("ID of the transaction to retrieve"))->required();
	getTransaction->add_option("-b,--block-hint", block_num_hint, localized("the block number this transaction may be in"));
	getTransaction->set_callback([&] {
		auto arg = fc::mutable_variant_object("id", transaction_id_str);
		if (block_num_hint > 0) {
			arg = arg("block_num_hint", block_num_hint);
		}
		std::cout << fc::json::to_pretty_string(call(get_transaction_func, arg)) << std::endl;
	});

	// get actions
	string account_name;
	string skip_seq_str;
	string num_seq_str;
	bool printjson = false;
	bool fullact = false;
	bool prettyact = false;
	bool printconsole = false;

	int32_t pos_seq = -1;
	int32_t offset = -20;
	auto getActions = get->add_subcommand("actions", localized("Retrieve all actions with specific account name referenced in authorization or receiver"), false);
	getActions->add_option("account_name", account_name, localized("name of account to query on"))->required();
	getActions->add_option("pos", pos_seq, localized("sequence number of action for this account, -1 for last"));
	getActions->add_option("offset", offset, localized("get actions [pos,pos+offset] for positive offset or [pos-offset,pos) for negative offset"));
	getActions->add_flag("--json,-j", printjson, localized("print full json"));
	getActions->add_flag("--full", fullact, localized("don't truncate action json"));
	getActions->add_flag("--pretty", prettyact, localized("pretty print full action json "));
	getActions->add_flag("--console", printconsole, localized("print console output generated by action "));
	getActions->set_callback([&] {
		fc::mutable_variant_object arg;
		arg("account_name", account_name);
		arg("pos", pos_seq);
		arg("offset", offset);

		auto result = call(get_actions_func, arg);


		if (printjson) {
			std::cout << fc::json::to_pretty_string(result) << std::endl;
		}
		else {
			auto& traces = result["actions"].get_array();
			uint32_t lib = result["last_irreversible_block"].as_uint64();


			cout << "#" << setw(5) << "seq" << "  " << setw(24) << left << "when" << "  " << setw(24) << right << "contract::action" << " => " << setw(13) << left << "receiver" << " " << setw(11) << left << "trx id..." << " args\n";
			cout << "================================================================================================================\n";
			for (const auto& trace : traces) {
				std::stringstream out;
				if (trace["block_num"].as_uint64() <= lib)
					out << "#";
				else
					out << "?";

				out << setw(5) << trace["account_action_seq"].as_uint64() << "  ";
				out << setw(24) << trace["block_time"].as_string() << "  ";

				const auto& at = trace["action_trace"].get_object();

				auto id = at["trx_id"].as_string();
				const auto& receipt = at["receipt"];
				auto receiver = receipt["receiver"].as_string();
				const auto& act = at["act"].get_object();
				auto code = act["account"].as_string();
				auto func = act["name"].as_string();
				string args;
				if (prettyact) {
					args = fc::json::to_pretty_string(act["data"]);
				}
				else {
					args = fc::json::to_string(act["data"]);
					if (!fullact) {
						args = args.substr(0, 60) + "...";
					}
				}
				out << std::setw(24) << std::right << (code + "::" + func) << " => " << left << std::setw(13) << receiver;

				out << " " << setw(11) << (id.substr(0, 8) + "...");

				if (fullact || prettyact) out << "\n";
				else out << " ";

				out << args;//<< "\n";

				if (trace["block_num"].as_uint64() <= lib) {
					dlog("\r${m}", ("m", out.str()));
				}
				else {
					wlog("\r${m}", ("m", out.str()));
				}
				if (printconsole) {
					auto console = at["console"].as_string();
					if (console.size()) {
						stringstream out;
						std::stringstream ss(console);
						string line;
						std::getline(ss, line);
						out << ">> " << line << "\n";
						cerr << out.str(); //ilog( "\r${m}                                   ", ("m",out.str()) );
					}
				}
			}
		}
	});

	auto getSchedule = get_schedule_subcommand{ get };
	auto getTransactionId = get_transaction_id_subcommand{ get };

	/*
	auto getTransactions = get->add_subcommand("transactions", localized("Retrieve all transactions with specific account name referenced in their scope"), false);
	getTransactions->add_option("account_name", account_name, localized("name of account to query on"))->required();
	getTransactions->add_option("skip_seq", skip_seq_str, localized("Number of most recent transactions to skip (0 would start at most recent transaction)"));
	getTransactions->add_option("num_seq", num_seq_str, localized("Number of transactions to return"));
	getTransactions->add_flag("--json,-j", printjson, localized("print full json"));
	getTransactions->set_callback([&] {
	   fc::mutable_variant_object arg;
	   if (skip_seq_str.empty()) {
		  arg = fc::mutable_variant_object( "account_name", account_name);
	   } else {
		  uint64_t skip_seq;
		  try {
			 skip_seq = boost::lexical_cast<uint64_t>(skip_seq_str);
		  } GST_RETHROW_EXCEPTIONS(chain_type_exception, "Invalid Skip Seq: ${skip_seq}", ("skip_seq", skip_seq_str))
		  if (num_seq_str.empty()) {
			 arg = fc::mutable_variant_object( "account_name", account_name)("skip_seq", skip_seq);
		  } else {
			 uint64_t num_seq;
			 try {
				num_seq = boost::lexical_cast<uint64_t>(num_seq_str);
			 } GST_RETHROW_EXCEPTIONS(chain_type_exception, "Invalid Num Seq: ${num_seq}", ("num_seq", num_seq_str))
			 arg = fc::mutable_variant_object( "account_name", account_name)("skip_seq", skip_seq_str)("num_seq", num_seq);
		  }
	   }
	   auto result = call(get_transactions_func, arg);
	   if( printjson ) {
		  std::cout << fc::json::to_pretty_string(result) << std::endl;
	   }
	   else {
		  const auto& trxs = result.get_object()["transactions"].get_array();
		  for( const auto& t : trxs ) {

			 const auto& tobj = t.get_object();
			 const auto& trx  = tobj["transaction"].get_object();
			 const auto& data = trx["transaction"].get_object();
			 const auto& msgs = data["actions"].get_array();

			 for( const auto& msg : msgs ) {
				int64_t seq_num  = tobj["seq_num"].as<int64_t>();
				string  id       = tobj["transaction_id"].as_string();
				const auto& exp  = data["expiration"].as<fc::time_point_sec>();
				std::cout << tobj["seq_num"].as_string() <<"] " << id.substr(0,8) << "...  " << data["expiration"].as_string() << "  ";
				auto code = msg["account"].as_string();
				auto func = msg["name"].as_string();
				auto args = fc::json::to_string( msg["data"] );
				std::cout << setw(26) << left << (code + "::" + func) << "  " << args;
				std::cout << std::endl;
			 }
		  }
	   }

	});
	*/

	// set subcommand
	auto setSubcommand = app.add_subcommand("set", localized("Set or update blockchain state"));
	setSubcommand->require_subcommand();

	// set contract subcommand
	string account;
	string contractPath;
	string wasmPath;
	string abiPath;
	bool shouldSend = true;
	bool contract_clear = false;
	bool suppress_duplicate_check = false;
	auto codeSubcommand = setSubcommand->add_subcommand("code", localized("Create or update the code on an account"));
	codeSubcommand->add_option("account", account, localized("The account to set code for"))->required();
	codeSubcommand->add_option("code-file", wasmPath, localized("The fullpath containing the contract WASM"));//->required();
	codeSubcommand->add_flag("-c,--clear", contract_clear, localized("Remove code on an account"));
	codeSubcommand->add_flag("--suppress-duplicate-check", suppress_duplicate_check, localized("Don't check for duplicate"));

	auto abiSubcommand = setSubcommand->add_subcommand("abi", localized("Create or update the abi on an account"));
	abiSubcommand->add_option("account", account, localized("The account to set the ABI for"))->required();
	abiSubcommand->add_option("abi-file", abiPath, localized("The fullpath containing the contract ABI"));//->required();
	abiSubcommand->add_flag("-c,--clear", contract_clear, localized("Remove abi on an account"));
	abiSubcommand->add_flag("--suppress-duplicate-check", suppress_duplicate_check, localized("Don't check for duplicate"));

	auto contractSubcommand = setSubcommand->add_subcommand("contract", localized("Create or update the contract on an account"));
	contractSubcommand->add_option("account", account, localized("The account to publish a contract for"))
		->required();
	contractSubcommand->add_option("contract-dir", contractPath, localized("The path containing the .wasm and .abi"));
	// ->required();
	contractSubcommand->add_option("wasm-file", wasmPath, localized("The file containing the contract WASM relative to contract-dir"));
	//                     ->check(CLI::ExistingFile);
	auto abi = contractSubcommand->add_option("abi-file,-a,--abi", abiPath, localized("The ABI for the contract relative to contract-dir"));
	//                                ->check(CLI::ExistingFile);
	contractSubcommand->add_flag("-c,--clear", contract_clear, localized("Rmove contract on an account"));
	contractSubcommand->add_flag("--suppress-duplicate-check", suppress_duplicate_check, localized("Don't check for duplicate"));

	std::vector<chain::action> actions;
	auto set_code_callback = [&]() {

		std::vector<char> old_wasm;
		bool duplicate = false;
		fc::sha256 old_hash, new_hash;
		if (!suppress_duplicate_check) {
			try {
				const auto result = call(get_code_hash_func, fc::mutable_variant_object("account_name", account));
				old_hash = fc::sha256(result["code_hash"].as_string());
			}
			catch (...) {
				std::cerr << "Failed to get existing code hash, continue without duplicate check..." << std::endl;
				suppress_duplicate_check = true;
			}
		}

		bytes code_bytes;
		if (!contract_clear) {
			std::string wasm;
			fc::path cpath(contractPath);

			if (cpath.filename().generic_string() == ".") cpath = cpath.parent_path();

			if (wasmPath.empty())
				wasmPath = (cpath / (cpath.filename().generic_string() + ".wasm")).generic_string();
			else
				wasmPath = (cpath / wasmPath).generic_string();

			std::cerr << localized(("Reading WASM from " + wasmPath + "...").c_str()) << std::endl;
			fc::read_file_contents(wasmPath, wasm);
			GST_ASSERT(!wasm.empty(), wast_file_not_found, "no wasm file found ${f}", ("f", wasmPath));

			const string binary_wasm_header("\x00\x61\x73\x6d\x01\x00\x00\x00", 8);
			if (wasm.compare(0, 8, binary_wasm_header))
				std::cerr << localized("WARNING: ") << wasmPath << localized(" doesn't look like a binary WASM file. Is it something else, like WAST? Trying anyways...") << std::endl;
			code_bytes = bytes(wasm.begin(), wasm.end());
		}
		else {
			code_bytes = bytes();
		}

		if (!suppress_duplicate_check) {
			if (code_bytes.size()) {
				new_hash = fc::sha256::hash(&(code_bytes[0]), code_bytes.size());
			}
			duplicate = (old_hash == new_hash);
		}

		if (!duplicate) {
			actions.emplace_back(create_setcode(account, code_bytes));
			if (shouldSend) {
				std::cerr << localized("Setting Code...") << std::endl;
				send_actions(std::move(actions), 10000, packed_transaction::zlib);
			}
		}
		else {
			std::cerr << localized("Skipping set code because the new code is the same as the existing code") << std::endl;
		}
	};

	auto set_abi_callback = [&]() {

		bytes old_abi;
		bool duplicate = false;
		if (!suppress_duplicate_check) {
			try {
				const auto result = call(get_raw_abi_func, fc::mutable_variant_object("account_name", account));
				old_abi = result["abi"].as_blob().data;
			}
			catch (...) {
				std::cerr << "Failed to get existing raw abi, continue without duplicate check..." << std::endl;
				suppress_duplicate_check = true;
			}
		}

		bytes abi_bytes;
		if (!contract_clear) {
			fc::path cpath(contractPath);
			if (cpath.filename().generic_string() == ".") cpath = cpath.parent_path();

			if (abiPath.empty()) {
				abiPath = (cpath / (cpath.filename().generic_string() + ".abi")).generic_string();
			}
			else {
				abiPath = (cpath / abiPath).generic_string();
			}

			GST_ASSERT(fc::exists(abiPath), abi_file_not_found, "no abi file found ${f}", ("f", abiPath));

			abi_bytes = fc::raw::pack(fc::json::from_file(abiPath).as<abi_def>());
		}
		else {
			abi_bytes = bytes();
		}

		if (!suppress_duplicate_check) {
			duplicate = (old_abi.size() == abi_bytes.size() && std::equal(old_abi.begin(), old_abi.end(), abi_bytes.begin()));
		}

		if (!duplicate) {
			try {
				actions.emplace_back(create_setabi(account, abi_bytes));
			} GST_RETHROW_EXCEPTIONS(abi_type_exception, "Fail to parse ABI JSON")
				if (shouldSend) {
					std::cerr << localized("Setting ABI...") << std::endl;
					send_actions(std::move(actions), 10000, packed_transaction::zlib);
				}
		}
		else {
			std::cerr << localized("Skipping set abi because the new abi is the same as the existing abi") << std::endl;
		}
	};

	add_standard_transaction_options(contractSubcommand, "account@active");
	add_standard_transaction_options(codeSubcommand, "account@active");
	add_standard_transaction_options(abiSubcommand, "account@active");
	contractSubcommand->set_callback([&] {
		if (!contract_clear) GST_ASSERT(!contractPath.empty(), contract_exception, " contract-dir is null ", ("f", contractPath));
		shouldSend = false;
		set_code_callback();
		set_abi_callback();
		if (actions.size()) {
			std::cerr << localized("Publishing contract...") << std::endl;
			send_actions(std::move(actions), 10000, packed_transaction::zlib);
		}
		else {
			std::cout << "no transaction is sent" << std::endl;
		}
	});
	codeSubcommand->set_callback(set_code_callback);
	abiSubcommand->set_callback(set_abi_callback);

	// set account
	auto setAccount = setSubcommand->add_subcommand("account", localized("set or update blockchain account state"))->require_subcommand();

	// set account permission
	auto setAccountPermission = set_account_permission_subcommand(setAccount);

	// set action
	auto setAction = setSubcommand->add_subcommand("action", localized("set or update blockchain action state"))->require_subcommand();

	// set action permission
	auto setActionPermission = set_action_permission_subcommand(setAction);

	// Transfer subcommand
	string con = "gstio.token";
	string sender;
	string recipient;
	string amount;
	string memo;
	bool pay_ram = false;
	auto transfer = app.add_subcommand("transfer", localized("Transfer GST from account to account"), false);
	transfer->add_option("sender", sender, localized("The account sending GST"))->required();
	transfer->add_option("recipient", recipient, localized("The account receiving GST"))->required();
	transfer->add_option("amount", amount, localized("The amount of GST to send"))->required();
	transfer->add_option("memo", memo, localized("The memo for the transfer"));
	transfer->add_option("--contract,-c", con, localized("The contract which controls the token"));
	transfer->add_flag("--pay-ram-to-open", pay_ram, localized("Pay ram to open recipient's token balance row"));

	add_standard_transaction_options(transfer, "sender@active");
	transfer->set_callback([&] {
		if (tx_force_unique && memo.size() == 0) {
			// use the memo to add a nonce
			memo = generate_nonce_string();
			tx_force_unique = false;
		}

		auto transfer_amount = to_asset(con, amount);
		auto transfer = create_transfer(con, sender, recipient, transfer_amount, memo);
		if (!pay_ram) {
			send_actions({ transfer });
		}
		else {
			auto open_ = create_open(con, recipient, transfer_amount.get_symbol(), sender);
			send_actions({ open_, transfer });
		}
	});

	// Net subcommand
	string new_host;
	auto net = app.add_subcommand("net", localized("Interact with local p2p network connections"), false);
	net->require_subcommand();
	auto connect = net->add_subcommand("connect", localized("start a new connection to a peer"), false);
	connect->add_option("host", new_host, localized("The hostname:port to connect to."))->required();
	connect->set_callback([&] {
		const auto& v = call(url, net_connect, new_host);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});

	auto disconnect = net->add_subcommand("disconnect", localized("close an existing connection"), false);
	disconnect->add_option("host", new_host, localized("The hostname:port to disconnect from."))->required();
	disconnect->set_callback([&] {
		const auto& v = call(url, net_disconnect, new_host);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});

	auto status = net->add_subcommand("status", localized("status of existing connection"), false);
	status->add_option("host", new_host, localized("The hostname:port to query status of connection"))->required();
	status->set_callback([&] {
		const auto& v = call(url, net_status, new_host);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});

	auto connections = net->add_subcommand("peers", localized("status of all existing peers"), false);
	connections->set_callback([&] {
		const auto& v = call(url, net_connections, new_host);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});



	// Wallet subcommand
	auto wallet = app.add_subcommand("wallet", localized("Interact with local wallet"), false);
	wallet->require_subcommand();
	// create wallet
	string wallet_name = "default";
	string password_file;
	auto createWallet = wallet->add_subcommand("create", localized("Create a new wallet locally"), false);
	createWallet->add_option("-n,--name", wallet_name, localized("The name of the new wallet"), true);
	createWallet->add_option("-f,--file", password_file, localized("Name of file to write wallet password output to. (Must be set, unless \"--to-console\" is passed"));
	createWallet->add_flag("--to-console", print_console, localized("Print password to console."));
	createWallet->set_callback([&wallet_name, &password_file, &print_console] {
		GSTC_ASSERT(!password_file.empty() ^ print_console, "ERROR: Either indicate a file using \"--file\" or pass \"--to-console\"");
		GSTC_ASSERT(password_file.empty() || !std::ofstream(password_file.c_str()).fail(), "ERROR: Failed to create file in specified path");

		const auto& v = call(wallet_url, wallet_create, wallet_name);
		std::cout << localized("Creating wallet: ${wallet_name}", ("wallet_name", wallet_name)) << std::endl;
		std::cout << localized("Save password to use in the future to unlock this wallet.") << std::endl;
		std::cout << localized("Without password imported keys will not be retrievable.") << std::endl;
		if (print_console) {
			std::cout << fc::json::to_pretty_string(v) << std::endl;
		}
		else {
			std::cerr << localized("saving password to ${filename}", ("filename", password_file)) << std::endl;
			auto password_str = fc::json::to_pretty_string(v);
			boost::replace_all(password_str, "\"", "");
			std::ofstream out(password_file.c_str());
			out << password_str;
		}
	});

	// open wallet
	auto openWallet = wallet->add_subcommand("open", localized("Open an existing wallet"), false);
	openWallet->add_option("-n,--name", wallet_name, localized("The name of the wallet to open"));
	openWallet->set_callback([&wallet_name] {
		call(wallet_url, wallet_open, wallet_name);
		std::cout << localized("Opened: ${wallet_name}", ("wallet_name", wallet_name)) << std::endl;
	});

	// lock wallet
	auto lockWallet = wallet->add_subcommand("lock", localized("Lock wallet"), false);
	lockWallet->add_option("-n,--name", wallet_name, localized("The name of the wallet to lock"));
	lockWallet->set_callback([&wallet_name] {
		call(wallet_url, wallet_lock, wallet_name);
		std::cout << localized("Locked: ${wallet_name}", ("wallet_name", wallet_name)) << std::endl;
	});

	// lock all wallets
	auto locakAllWallets = wallet->add_subcommand("lock_all", localized("Lock all unlocked wallets"), false);
	locakAllWallets->set_callback([] {
		call(wallet_url, wallet_lock_all);
		std::cout << localized("Locked All Wallets") << std::endl;
	});

	// unlock wallet
	string wallet_pw;
	auto unlockWallet = wallet->add_subcommand("unlock", localized("Unlock wallet"), false);
	unlockWallet->add_option("-n,--name", wallet_name, localized("The name of the wallet to unlock"));
	unlockWallet->add_option("--password", wallet_pw, localized("The password returned by wallet create"));
	unlockWallet->set_callback([&wallet_name, &wallet_pw] {
		prompt_for_wallet_password(wallet_pw, wallet_name);

		fc::variants vs = { fc::variant(wallet_name), fc::variant(wallet_pw) };
		call(wallet_url, wallet_unlock, vs);
		std::cout << localized("Unlocked: ${wallet_name}", ("wallet_name", wallet_name)) << std::endl;
	});

	// import keys into wallet
	string wallet_key_str;
	auto importWallet = wallet->add_subcommand("import", localized("Import private key into wallet"), false);
	importWallet->add_option("-n,--name", wallet_name, localized("The name of the wallet to import key into"));
	importWallet->add_option("--private-key", wallet_key_str, localized("Private key in WIF format to import"));
	importWallet->set_callback([&wallet_name, &wallet_key_str] {
		if (wallet_key_str.size() == 0) {
			std::cout << localized("private key: ");
			fc::set_console_echo(false);
			std::getline(std::cin, wallet_key_str, '\n');
			fc::set_console_echo(true);
		}

		private_key_type wallet_key;
		try {
			wallet_key = private_key_type(wallet_key_str);
		}
		catch (...) {
			GST_THROW(private_key_type_exception, "Invalid private key: ${private_key}", ("private_key", wallet_key_str))
		}
		public_key_type pubkey = wallet_key.get_public_key();

		fc::variants vs = { fc::variant(wallet_name), fc::variant(wallet_key) };
		call(wallet_url, wallet_import_key, vs);
		std::cout << localized("imported private key for: ${pubkey}", ("pubkey", std::string(pubkey))) << std::endl;
	});

	// remove keys from wallet
	string wallet_rm_key_str;
	auto removeKeyWallet = wallet->add_subcommand("remove_key", localized("Remove key from wallet"), false);
	removeKeyWallet->add_option("-n,--name", wallet_name, localized("The name of the wallet to remove key from"));
	removeKeyWallet->add_option("key", wallet_rm_key_str, localized("Public key in WIF format to remove"))->required();
	removeKeyWallet->add_option("--password", wallet_pw, localized("The password returned by wallet create"));
	removeKeyWallet->set_callback([&wallet_name, &wallet_pw, &wallet_rm_key_str] {
		prompt_for_wallet_password(wallet_pw, wallet_name);
		public_key_type pubkey;
		try {
			pubkey = public_key_type(wallet_rm_key_str);
		}
		catch (...) {
			GST_THROW(public_key_type_exception, "Invalid public key: ${public_key}", ("public_key", wallet_rm_key_str))
		}
		fc::variants vs = { fc::variant(wallet_name), fc::variant(wallet_pw), fc::variant(wallet_rm_key_str) };
		call(wallet_url, wallet_remove_key, vs);
		std::cout << localized("removed private key for: ${pubkey}", ("pubkey", wallet_rm_key_str)) << std::endl;
	});

	// create a key within wallet
	string wallet_create_key_type;
	auto createKeyInWallet = wallet->add_subcommand("create_key", localized("Create private key within wallet"), false);
	createKeyInWallet->add_option("-n,--name", wallet_name, localized("The name of the wallet to create key into"), true);
	createKeyInWallet->add_option("key_type", wallet_create_key_type, localized("Key type to create (K1/R1)"), true)->set_type_name("K1/R1");
	createKeyInWallet->set_callback([&wallet_name, &wallet_create_key_type] {
		//an empty key type is allowed -- it will let the underlying wallet pick which type it prefers
		fc::variants vs = { fc::variant(wallet_name), fc::variant(wallet_create_key_type) };
		const auto& v = call(wallet_url, wallet_create_key, vs);
		std::cout << localized("Created new private key with a public key of: ") << fc::json::to_pretty_string(v) << std::endl;
	});

	// list wallets
	auto listWallet = wallet->add_subcommand("list", localized("List opened wallets, * = unlocked"), false);
	listWallet->set_callback([] {
		std::cout << localized("Wallets:") << std::endl;
		const auto& v = call(wallet_url, wallet_list);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});

	// list keys
	auto listKeys = wallet->add_subcommand("keys", localized("List of public keys from all unlocked wallets."), false);
	listKeys->set_callback([] {
		const auto& v = call(wallet_url, wallet_public_keys);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});

	// list private keys
	auto listPrivKeys = wallet->add_subcommand("private_keys", localized("List of private keys from an unlocked wallet in wif or PVT_R1 format."), false);
	listPrivKeys->add_option("-n,--name", wallet_name, localized("The name of the wallet to list keys from"), true);
	listPrivKeys->add_option("--password", wallet_pw, localized("The password returned by wallet create"));
	listPrivKeys->set_callback([&wallet_name, &wallet_pw] {
		prompt_for_wallet_password(wallet_pw, wallet_name);
		fc::variants vs = { fc::variant(wallet_name), fc::variant(wallet_pw) };
		const auto& v = call(wallet_url, wallet_list_keys, vs);
		std::cout << fc::json::to_pretty_string(v) << std::endl;
	});

	auto stopKgstd = wallet->add_subcommand("stop", localized("Stop kgstd (doesn't work with nodgst)."), false);
	stopKgstd->set_callback([] {
		const auto& v = call(wallet_url, kgstd_stop);
		if (!v.is_object() || v.get_object().size() != 0) { //on success kgstd responds with empty object
			std::cerr << fc::json::to_pretty_string(v) << std::endl;
		}
		else {
			std::cout << "OK" << std::endl;
		}
	});

	// sign subcommand
	string trx_json_to_sign;
	string str_private_key;
	string str_chain_id;
	bool push_trx = false;

	auto sign = app.add_subcommand("sign", localized("Sign a transaction"), false);
	sign->add_option("transaction", trx_json_to_sign,
		localized("The JSON string or filename defining the transaction to sign"), true)->required();
	sign->add_option("-k,--private-key", str_private_key, localized("The private key that will be used to sign the transaction"));
	sign->add_option("-c,--chain-id", str_chain_id, localized("The chain id that will be used to sign the transaction"));
	sign->add_flag("-p,--push-transaction", push_trx, localized("Push transaction after signing"));

	sign->set_callback([&] {
		signed_transaction trx = json_from_file_or_string(trx_json_to_sign).as<signed_transaction>();

		fc::optional<chain_id_type> chain_id;

		if (str_chain_id.size() == 0) {
			ilog("grabbing chain_id from nodgst");
			auto info = get_info();
			chain_id = info.chain_id;
		}
		else {
			chain_id = chain_id_type(str_chain_id);
		}

		if (str_private_key.size() == 0) {
			std::cerr << localized("private key: ");
			fc::set_console_echo(false);
			std::getline(std::cin, str_private_key, '\n');
			fc::set_console_echo(true);
		}

		auto priv_key = fc::crypto::private_key::regenerate(*utilities::wif_to_key(str_private_key));
		trx.sign(priv_key, *chain_id);

		if (push_trx) {
			auto trx_result = call(push_txn_func, packed_transaction(trx, packed_transaction::none));
			std::cout << fc::json::to_pretty_string(trx_result) << std::endl;
		}
		else {
			std::cout << fc::json::to_pretty_string(trx) << std::endl;
		}
	});

	// Push subcommand
	auto push = app.add_subcommand("push", localized("Push arbitrary transactions to the blockchain"), false);
	push->require_subcommand();

	// push action
	string contract_account;
	string action;
	string data;
	vector<string> permissions;
	auto actionsSubcommand = push->add_subcommand("action", localized("Push a transaction with a single action"));
	actionsSubcommand->fallthrough(false);
	actionsSubcommand->add_option("account", contract_account,
		localized("The account providing the contract to execute"), true)->required();
	actionsSubcommand->add_option("action", action,
		localized("A JSON string or filename defining the action to execute on the contract"), true)->required();
	actionsSubcommand->add_option("data", data, localized("The arguments to the contract"))->required();

	add_standard_transaction_options(actionsSubcommand);
	actionsSubcommand->set_callback([&] {
		fc::variant action_args_var;
		if (!data.empty()) {
			try {
				action_args_var = json_from_file_or_string(data, fc::json::relaxed_parser);
			} GST_RETHROW_EXCEPTIONS(action_type_exception, "Fail to parse action JSON data='${data}'", ("data", data))
		}
		auto accountPermissions = get_account_permissions(tx_permission);

		send_actions({ chain::action{accountPermissions, contract_account, action, variant_to_bin(contract_account, action, action_args_var) } });
	});

	// push transaction
	string trx_to_push;
	auto trxSubcommand = push->add_subcommand("transaction", localized("Push an arbitrary JSON transaction"));
	trxSubcommand->add_option("transaction", trx_to_push, localized("The JSON string or filename defining the transaction to push"))->required();
	add_standard_transaction_options(trxSubcommand);

	trxSubcommand->set_callback([&] {
		fc::variant trx_var;
		try {
			trx_var = json_from_file_or_string(trx_to_push);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse transaction JSON '${data}'", ("data", trx_to_push))
			try {
			signed_transaction trx = trx_var.as<signed_transaction>();
			std::cout << fc::json::to_pretty_string(push_transaction(trx)) << std::endl;
		}
		catch (fc::exception&) {
			// unable to convert so try via abi
			signed_transaction trx;
			abi_serializer::from_variant(trx_var, trx, abi_serializer_resolver, abi_serializer_max_time);
			std::cout << fc::json::to_pretty_string(push_transaction(trx)) << std::endl;
		}
	});


	string trxsJson;
	auto trxsSubcommand = push->add_subcommand("transactions", localized("Push an array of arbitrary JSON transactions"));
	trxsSubcommand->add_option("transactions", trxsJson, localized("The JSON string or filename defining the array of the transactions to push"))->required();
	trxsSubcommand->set_callback([&] {
		fc::variant trx_var;
		try {
			trx_var = json_from_file_or_string(trxsJson);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse transaction JSON '${data}'", ("data", trxsJson))
			auto trxs_result = call(push_txns_func, trx_var);
		std::cout << fc::json::to_pretty_string(trxs_result) << std::endl;
	});


	// multisig subcommand
	auto msig = app.add_subcommand("multisig", localized("Multisig contract commands"), false);
	msig->require_subcommand();

	// multisig propose
	string proposal_name;
	string requested_perm;
	string transaction_perm;
	string proposed_transaction;
	string proposed_contract;
	string proposed_action;
	string proposer;
	unsigned int proposal_expiration_hours = 24;
	CLI::callback_t parse_expiration_hours = [&](CLI::results_t res) -> bool {
		unsigned int value_s;
		if (res.size() == 0 || !CLI::detail::lexical_cast(res[0], value_s)) {
			return false;
		}

		proposal_expiration_hours = static_cast<uint64_t>(value_s);
		return true;
	};

	auto propose_action = msig->add_subcommand("propose", localized("Propose action"));
	add_standard_transaction_options(propose_action);
	propose_action->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();
	propose_action->add_option("requested_permissions", requested_perm, localized("The JSON string or filename defining requested permissions"))->required();
	propose_action->add_option("trx_permissions", transaction_perm, localized("The JSON string or filename defining transaction permissions"))->required();
	propose_action->add_option("contract", proposed_contract, localized("contract to which deferred transaction should be delivered"))->required();
	propose_action->add_option("action", proposed_action, localized("action of deferred transaction"))->required();
	propose_action->add_option("data", proposed_transaction, localized("The JSON string or filename defining the action to propose"))->required();
	propose_action->add_option("proposer", proposer, localized("Account proposing the transaction"));
	propose_action->add_option("proposal_expiration", parse_expiration_hours, localized("Proposal expiration interval in hours"));

	propose_action->set_callback([&] {
		fc::variant requested_perm_var;
		try {
			requested_perm_var = json_from_file_or_string(requested_perm);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse permissions JSON '${data}'", ("data", requested_perm))
			fc::variant transaction_perm_var;
		try {
			transaction_perm_var = json_from_file_or_string(transaction_perm);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse permissions JSON '${data}'", ("data", transaction_perm))
			fc::variant trx_var;
		try {
			trx_var = json_from_file_or_string(proposed_transaction);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse transaction JSON '${data}'", ("data", proposed_transaction))
			transaction proposed_trx = trx_var.as<transaction>();
		bytes proposed_trx_serialized = variant_to_bin(proposed_contract, proposed_action, trx_var);

		vector<permission_level> reqperm;
		try {
			reqperm = requested_perm_var.as<vector<permission_level>>();
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Wrong requested permissions format: '${data}'", ("data", requested_perm_var));

		vector<permission_level> trxperm;
		try {
			trxperm = transaction_perm_var.as<vector<permission_level>>();
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Wrong transaction permissions format: '${data}'", ("data", transaction_perm_var));

		auto accountPermissions = get_account_permissions(tx_permission);
		if (accountPermissions.empty()) {
			if (!proposer.empty()) {
				accountPermissions = vector<permission_level>{ {proposer, config::active_name} };
			}
			else {
				GST_THROW(missing_auth_exception, "Authority is not provided (either by multisig parameter <proposer> or -p)");
			}
		}
		if (proposer.empty()) {
			proposer = name(accountPermissions.at(0).actor).to_string();
		}

		transaction trx;

		trx.expiration = fc::time_point_sec(fc::time_point::now() + fc::hours(proposal_expiration_hours));
		trx.ref_block_num = 0;
		trx.ref_block_prefix = 0;
		trx.max_net_usage_words = 0;
		trx.max_cpu_usage_ms = 0;
		trx.delay_sec = 0;
		trx.actions = { chain::action(trxperm, name(proposed_contract), name(proposed_action), proposed_trx_serialized) };

		fc::to_variant(trx, trx_var);

		auto args = fc::mutable_variant_object()
			("proposer", proposer)
			("proposal_name", proposal_name)
			("requested", requested_perm_var)
			("trx", trx_var);

		send_actions({ chain::action{accountPermissions, "gstio.msig", "propose", variant_to_bin(N(gstio.msig), N(propose), args) } });
	});

	//multisige propose transaction
	auto propose_trx = msig->add_subcommand("propose_trx", localized("Propose transaction"));
	add_standard_transaction_options(propose_trx);
	propose_trx->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();
	propose_trx->add_option("requested_permissions", requested_perm, localized("The JSON string or filename defining requested permissions"))->required();
	propose_trx->add_option("transaction", trx_to_push, localized("The JSON string or filename defining the transaction to push"))->required();
	propose_trx->add_option("proposer", proposer, localized("Account proposing the transaction"));

	propose_trx->set_callback([&] {
		fc::variant requested_perm_var;
		try {
			requested_perm_var = json_from_file_or_string(requested_perm);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse permissions JSON '${data}'", ("data", requested_perm))

			fc::variant trx_var;
		try {
			trx_var = json_from_file_or_string(trx_to_push);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse transaction JSON '${data}'", ("data", trx_to_push))

			auto accountPermissions = get_account_permissions(tx_permission);
		if (accountPermissions.empty()) {
			if (!proposer.empty()) {
				accountPermissions = vector<permission_level>{ {proposer, config::active_name} };
			}
			else {
				GST_THROW(missing_auth_exception, "Authority is not provided (either by multisig parameter <proposer> or -p)");
			}
		}
		if (proposer.empty()) {
			proposer = name(accountPermissions.at(0).actor).to_string();
		}

		auto args = fc::mutable_variant_object()
			("proposer", proposer)
			("proposal_name", proposal_name)
			("requested", requested_perm_var)
			("trx", trx_var);

		send_actions({ chain::action{accountPermissions, "gstio.msig", "propose", variant_to_bin(N(gstio.msig), N(propose), args) } });
	});


	// multisig review
	auto review = msig->add_subcommand("review", localized("Review transaction"));
	review->add_option("proposer", proposer, localized("proposer name (string)"))->required();
	review->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();

	review->set_callback([&] {
		auto result = call(get_table_func, fc::mutable_variant_object("json", true)
			("code", "gstio.msig")
			("scope", proposer)
			("table", "proposal")
			("table_key", "")
			("lower_bound", gstio::chain::string_to_name(proposal_name.c_str()))
			("upper_bound", "")
			("limit", 1)
		);
		//std::cout << fc::json::to_pretty_string(result) << std::endl;

		fc::variants rows = result.get_object()["rows"].get_array();
		if (rows.empty()) {
			std::cerr << "Proposal not found" << std::endl;
			return;
		}
		fc::mutable_variant_object obj = rows[0].get_object();
		if (obj["proposal_name"] != proposal_name) {
			std::cerr << "Proposal not found" << std::endl;
			return;
		}
		auto trx_hex = obj["packed_transaction"].as_string();
		vector<char> trx_blob(trx_hex.size() / 2);
		fc::from_hex(trx_hex, trx_blob.data(), trx_blob.size());
		transaction trx = fc::raw::unpack<transaction>(trx_blob);

		fc::variant trx_var;
		abi_serializer abi;
		abi.to_variant(trx, trx_var, abi_serializer_resolver, abi_serializer_max_time);
		obj["transaction"] = trx_var;
		std::cout << fc::json::to_pretty_string(obj)
			<< std::endl;
	});

	string perm;
	auto approve_or_unapprove = [&](const string& action) {
		fc::variant perm_var;
		try {
			perm_var = json_from_file_or_string(perm);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse permissions JSON '${data}'", ("data", perm))
			auto args = fc::mutable_variant_object()
			("proposer", proposer)
			("proposal_name", proposal_name)
			("level", perm_var);

		auto accountPermissions = tx_permission.empty() ? vector<chain::permission_level>{ {sender, config::active_name}} : get_account_permissions(tx_permission);
		send_actions({ chain::action{accountPermissions, "gstio.msig", action, variant_to_bin(N(gstio.msig), action, args) } });
	};

	// multisig approve
	auto approve = msig->add_subcommand("approve", localized("Approve proposed transaction"));
	add_standard_transaction_options(approve);
	approve->add_option("proposer", proposer, localized("proposer name (string)"))->required();
	approve->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();
	approve->add_option("permissions", perm, localized("The JSON string of filename defining approving permissions"))->required();
	approve->set_callback([&] { approve_or_unapprove("approve"); });

	// multisig unapprove
	auto unapprove = msig->add_subcommand("unapprove", localized("Unapprove proposed transaction"));
	add_standard_transaction_options(unapprove);
	unapprove->add_option("proposer", proposer, localized("proposer name (string)"))->required();
	unapprove->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();
	unapprove->add_option("permissions", perm, localized("The JSON string of filename defining approving permissions"))->required();
	unapprove->set_callback([&] { approve_or_unapprove("unapprove"); });

	// multisig cancel
	string canceler;
	auto cancel = msig->add_subcommand("cancel", localized("Cancel proposed transaction"));
	add_standard_transaction_options(cancel);
	cancel->add_option("proposer", proposer, localized("proposer name (string)"))->required();
	cancel->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();
	cancel->add_option("canceler", canceler, localized("canceler name (string)"));
	cancel->set_callback([&]() {
		auto accountPermissions = get_account_permissions(tx_permission);
		if (accountPermissions.empty()) {
			if (!canceler.empty()) {
				accountPermissions = vector<permission_level>{ {canceler, config::active_name} };
			}
			else {
				GST_THROW(missing_auth_exception, "Authority is not provided (either by multisig parameter <canceler> or -p)");
			}
		}
		if (canceler.empty()) {
			canceler = name(accountPermissions.at(0).actor).to_string();
		}
		auto args = fc::mutable_variant_object()
			("proposer", proposer)
			("proposal_name", proposal_name)
			("canceler", canceler);

		send_actions({ chain::action{accountPermissions, "gstio.msig", "cancel", variant_to_bin(N(gstio.msig), N(cancel), args) } });
	}
	);

	// multisig exec
	string executer;
	auto exec = msig->add_subcommand("exec", localized("Execute proposed transaction"));
	add_standard_transaction_options(exec);
	exec->add_option("proposer", proposer, localized("proposer name (string)"))->required();
	exec->add_option("proposal_name", proposal_name, localized("proposal name (string)"))->required();
	exec->add_option("executer", executer, localized("account paying for execution (string)"));
	exec->set_callback([&] {
		auto accountPermissions = get_account_permissions(tx_permission);
		if (accountPermissions.empty()) {
			if (!executer.empty()) {
				accountPermissions = vector<permission_level>{ {executer, config::active_name} };
			}
			else {
				GST_THROW(missing_auth_exception, "Authority is not provided (either by multisig parameter <executer> or -p)");
			}
		}
		if (executer.empty()) {
			executer = name(accountPermissions.at(0).actor).to_string();
		}

		auto args = fc::mutable_variant_object()
			("proposer", proposer)
			("proposal_name", proposal_name)
			("executer", executer);

		send_actions({ chain::action{accountPermissions, "gstio.msig", "exec", variant_to_bin(N(gstio.msig), N(exec), args) } });
	}
	);

	// wrap subcommand
	auto wrap = app.add_subcommand("wrap", localized("Wrap contract commands"), false);
	wrap->require_subcommand();

	// wrap exec
	string wrap_con = "gstio.wrap";
	executer = "";
	string trx_to_exec;
	auto wrap_exec = wrap->add_subcommand("exec", localized("Execute a transaction while bypassing authorization checks"));
	add_standard_transaction_options(wrap_exec);
	wrap_exec->add_option("executer", executer, localized("Account executing the transaction and paying for the deferred transaction RAM"))->required();
	wrap_exec->add_option("transaction", trx_to_exec, localized("The JSON string or filename defining the transaction to execute"))->required();
	wrap_exec->add_option("--contract,-c", wrap_con, localized("The account which controls the wrap contract"));

	wrap_exec->set_callback([&] {
		fc::variant trx_var;
		try {
			trx_var = json_from_file_or_string(trx_to_exec);
		} GST_RETHROW_EXCEPTIONS(transaction_type_exception, "Fail to parse transaction JSON '${data}'", ("data", trx_to_exec))

			auto accountPermissions = get_account_permissions(tx_permission);
		if (accountPermissions.empty()) {
			accountPermissions = vector<permission_level>{ {executer, config::active_name}, {wrap_con, config::active_name} };
		}

		auto args = fc::mutable_variant_object()
			("executer", executer)
			("trx", trx_var);

		send_actions({ chain::action{accountPermissions, wrap_con, "exec", variant_to_bin(wrap_con, N(exec), args) } });
	});

	// system subcommand
	auto system = app.add_subcommand("system", localized("Send gstio.system contract action to the blockchain."), false);
	system->require_subcommand();

	auto createAccountSystem = create_account_subcommand(system, false /*simple*/);
	auto registerProducer = register_producer_subcommand(system);
	auto unregisterProducer = unregister_producer_subcommand(system);

	auto voteProducer = system->add_subcommand("voteproducer", localized("Vote for a producer"));
	voteProducer->require_subcommand();
	auto voteProxy = vote_producer_proxy_subcommand(voteProducer);
	auto voteProducers = vote_producers_subcommand(voteProducer);
	auto approveProducer = approve_producer_subcommand(voteProducer);
	auto unapproveProducer = unapprove_producer_subcommand(voteProducer);

	auto listProducers = list_producers_subcommand(system);

	auto delegateBandWidth = delegate_bandwidth_subcommand(system);
	auto undelegateBandWidth = undelegate_bandwidth_subcommand(system);
	auto listBandWidth = list_bw_subcommand(system);
	auto bidname = bidname_subcommand(system);
	auto bidnameinfo = bidname_info_subcommand(system);

	// auto biyram = buyram_subcommand(system);
	 //auto sellram = sellram_subcommand(system);

	auto claimRewards = claimrewards_subcommand(system);
	auto prodRewards = prodrewards_subcommand(system);	//2019/03/12
	auto claimvrew = claimvrew_subcommand(system);		//2019/03/11
	auto voteRewards = voterewards_subcommand(system);
	auto regProxy = regproxy_subcommand(system);
	auto unregProxy = unregproxy_subcommand(system);

	auto cancelDelay = canceldelay_subcommand(system);

	try {
		app.parse(argc, argv);
	}
	catch (const CLI::ParseError &e) {
		return app.exit(e);
	}
	catch (const explained_exception& e) {
		return 1;
	}
	catch (connection_exception& e) {
		if (verbose_errors) {
			elog("connect error: ${e}", ("e", e.to_detail_string()));
		}
		return 1;
	}
	catch (const fc::exception& e) {
		// attempt to extract the error code if one is present
		if (!print_recognized_errors(e, verbose_errors)) {
			// Error is not recognized
			if (!print_help_text(e) || verbose_errors) {
				elog("Failed with error: ${e}", ("e", verbose_errors ? e.to_detail_string() : e.to_string()));
			}
		}
		return 1;
	}

	return 0;
}
