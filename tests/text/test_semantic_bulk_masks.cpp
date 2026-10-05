#include "text/structured_output.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
using namespace ninfer;
using namespace ninfer::text;
int main(){try{
 std::vector<std::string> vocab(257);
 for(int i=0;i<256;++i)vocab[i]=std::string(1,static_cast<char>(i));
 const auto add=[&](std::string text){auto id=static_cast<TokenId>(vocab.size());vocab.push_back(std::move(text));return id;};
 std::vector<TokenId> candidates;
 for(const auto& text:std::vector<std::string>{"a","ab","abcd",R"(\u0061)",R"(\ud83d\ude00)","\"", "\"}","abac\"x\"","abac\"xy\""})candidates.push_back(add(text));
 StructuredCompiler compiler(vocab,{256});
 const auto check=[&](GrammarState& state){
   std::vector<std::uint32_t> mask((vocab.size()+31)/32);state.fill_masks(mask,{});
   for(TokenId id:candidates){
     auto trial=state.fork();bool accepted=true;try{trial->accept(std::span<const TokenId>(&id,1));}catch(...){accepted=false;}
     bool allowed=(mask[id/32]>>(id%32))&1;
     if(accepted!=allowed)throw std::runtime_error("mask differs from definitive accept for token "+std::to_string(id));
   }
 };
 const auto bytes=[](std::string text){std::vector<TokenId> ids;for(unsigned char c:text)ids.push_back(c);return ids;};
 for(int limit:{0,1,3,180}){
   auto state=compiler.compile({StructuredOutputKind::JsonSchema,"{\"type\":\"string\",\"maxLength\":"+std::to_string(limit)+"}"});
   state->accept(bytes("\""));check(*state);
   if(limit){state->accept(bytes("a"));check(*state);}
 }
 auto utf=compiler.compile({StructuredOutputKind::JsonSchema,R"({"type":"string","minLength":1,"maxLength":1})"});
 utf->accept(bytes(std::string("\"")+char(0xf0)));check(*utf);
 utf->accept(bytes(std::string{char(0x9f),char(0x98),char(0x80)}));check(*utf);
 StructuredOutputEnvelope envelope;envelope.reasoning_close="ababac";
 auto reasoning=compiler.compile({StructuredOutputKind::JsonSchema,R"({"type":"string","minLength":1,"maxLength":1})"},envelope);
 reasoning->accept(bytes("abab"));check(*reasoning);
 auto good=reasoning->fork();TokenId valid=candidates[candidates.size()-2];good->accept(std::span<const TokenId>(&valid,1));
 auto bad=reasoning->fork();TokenId invalid=candidates.back();bool rejected=false;
 try{bad->accept(std::span<const TokenId>(&invalid,1));}catch(...){rejected=true;}
 if(!rejected)throw std::runtime_error("overlapping reasoning delimiter bypassed maxLength");
 std::cout<<"Bulk mask/scalar/escape/fork/overlapping-delimiter checks passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
