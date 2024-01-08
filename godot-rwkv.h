#ifndef RWKV_GODOT_H
#define RWKV_GODOT_H


#undef VK_USE_PLATFORM_XLIB_KHR


#include "rwkv.h"
#include "tokenizer/tokenizer.hpp"
#include "sampler/sample.h"




#include "core/io/resource.h"
#include "core/object/ref_counted.h"






class Agent : public Resource {
	GDCLASS(Agent, Resource);

	public:
	std::map<std::string, Tensor> state = {};
	std::vector<std::string> stop_sequences = {};
	size_t max_queued_tokens = 0;
	float temperature = 0.9;
	float tau = 0.7;
	size_t last_token = 187;
	RWKVTokenizer* tokenizer = nullptr;
	std::vector<size_t> context = {};
	std::string add_context_queue = "";
	bool busy = false;
	
	Agent(RWKV* model, RWKVTokenizer* tokenizeri) {
		state = model->new_state();
		tokenizer = tokenizeri;
	}

	Agent() {
	}

	size_t add_context(String contexta) {
		// assert that add_context_queue is empty
		// assert that max_queued_tokens is 0
		if (max_queued_tokens != 0 || add_context_queue != "" || busy) {
			ERR_PRINT("add_context_queue is not empty or max_queued_tokens is not 0");
			return -1;
		}

		add_context_queue = std::string(contexta.utf8().get_data());
		auto tokens = tokenizer->encode(add_context_queue);
		context.clear();
		busy = true;
		return 0;
	}

	bool is_busy() {
		return busy;
	}

	void generate(size_t tokens){
		max_queued_tokens = tokens;	
	}

	void set_temperature(float temp) {
		temperature = temp;
	}

	void set_tau(float t) {
		tau = t;
	}

	void set_stop_sequences(Array sequences) {
		for (int i = 0; i < sequences.size(); i++) {
			stop_sequences.push_back(std::string(sequences[i].operator String().utf8().get_data()));
		}
	}

	void set_last_token(size_t token) {
		last_token = token;
	}

	// threadsafe return context
	String get_context() {
		if (context.size() == 0) {
			return "";
		}
		return String(tokenizer->decode(context).c_str());
	}

	// get last token
	size_t get_last_token() {
		return last_token;
	}

	// get max queued tokens
	size_t get_max_queued_tokens() {
		return max_queued_tokens;
	}

	protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("add_context"), &Agent::add_context);
		ClassDB::bind_method(D_METHOD("generate"), &Agent::generate);
		ClassDB::bind_method(D_METHOD("set_temperature"), &Agent::set_temperature);
		ClassDB::bind_method(D_METHOD("set_tau"), &Agent::set_tau);
		ClassDB::bind_method(D_METHOD("set_stop_sequences"), &Agent::set_stop_sequences);
		ClassDB::bind_method(D_METHOD("set_last_token"), &Agent::set_last_token);
		ClassDB::bind_method(D_METHOD("get_context"), &Agent::get_context);
		ClassDB::bind_method(D_METHOD("get_last_token"), &Agent::get_last_token);
		ClassDB::bind_method(D_METHOD("get_max_queued_tokens"), &Agent::get_max_queued_tokens);
	}

};

class GodotRWKV : public Resource {
	GDCLASS(GodotRWKV, Resource);

	


public:
	RWKV* model = nullptr;
	RWKVTokenizer* tokenizer = nullptr;
	size_t lastToken = 187;
	size_t max_agents = 50;
	std::vector<Agent*> agents = {};
	GodotRWKV() {
		
	}

	void loadModel(String path, size_t max_batch = 50) {
		max_agents = max_batch;
		// model.loadFile(std::string(path.utf8().get_data()));
		model = new RWKV(std::string(path.utf8().get_data()));
	};

	void loadTokenizer(String path) {
		// model.loadFile(std::string(path.utf8().get_data()));
		tokenizer = new RWKVTokenizer(std::string(path.utf8().get_data()));
	};

	
	void listen() {
		
			// sleep
			// do context processing
			if (agents.size() > 0) {
				std::vector<Agent*> toProcess = {};
				for (size_t i = 0; i < agents.size(); i++) {
					if (agents[i]->add_context_queue != "") {
						std::cout << "processing context" << std::endl;
						auto tokens = tokenizer->encode(agents[i]->add_context_queue);
						std::cout << "tokens: " << tokens.size() << std::endl;
						model->set_state(agents[i]->state, 0);
						std::cout << "state set" << std::endl;

						auto maxBatchSeqSize = max_agents;

						// process tokens in batches of maxBatchSeqSize
						for (size_t oi = 0; oi < tokens.size()-1; oi += maxBatchSeqSize) {
							auto tokensBatch = std::vector<size_t>();
							tokensBatch.push_back(agents[i]->last_token);
							for (size_t j = oi; j < MIN(oi + maxBatchSeqSize, tokens.size()-1); j++) {
								tokensBatch.push_back(tokens[j]);
							}
							std::cout << "tokensBatch: " << oi << std::endl;
							auto outputs = (*model)({tokensBatch});
							if (oi + maxBatchSeqSize >= tokens.size()-1) {
								agents[i]->last_token = tokens[tokens.size()-1];
							}
						}
						std::cout << "context processed" << std::endl;

						agents[i]->add_context_queue = "";
						std::cout << "context processed" << std::endl;
						agents[i]->busy = false;
						std::cout << "context processed busy" << std::endl;

						// std::cout << "context processed" << std::endl;
						model->get_state(agents[i]->state, 0);
						std::cout << "agent state retrieved" << std::endl;
					}

					if (agents[i]->max_queued_tokens > 0) {
						toProcess.push_back(agents[i]);
						agents[i]->busy = true;
					}
				}

				std::vector<std::vector<size_t>> tokens = {};

				for (size_t i = 0; i < toProcess.size(); i++) {
					tokens.push_back({toProcess[i]->last_token});
					model->set_state(toProcess[i]->state, i);
				}

				if (tokens.size() == 0) {
					return;
				}

				std::cout << "tokens: " << tokens.size() << std::endl;

				auto outputs = (*model)(tokens);
				// outputs.reshape({outputs.shape[0], size_t(pow(2, 16))});
				std::cout << "outputs: " << outputs.shape[0] << ":" << outputs.shape[1] << ":" << outputs.shape[2] << std::endl;

				for (size_t i = 0; i < toProcess.size(); i++) {
					auto out = outputs[i];
					auto token = typical(flp(out.data), toProcess[i]->temperature, toProcess[i]->tau);
					
					toProcess[i]->max_queued_tokens -= 1;
					

					// check if stop sequence
					bool stopped = (token == 0);
					if ((toProcess[i]->context.size() > 5) && token != 0){
						std::cout << "last token: " << token << std::endl;
						auto tokstocheck = std::vector<size_t>(toProcess[i]->context.end()-5, toProcess[i]->context.end());
						tokstocheck.push_back(token);
						std::string context5toks = tokenizer->decode(tokstocheck);
						for (size_t j = 0; j < toProcess[i]->stop_sequences.size(); j++) {
							auto stop_sequence = toProcess[i]->stop_sequences[j];
							
							if (context5toks.find(stop_sequence) != std::string::npos) {
								toProcess[i]->max_queued_tokens = 0;
								stopped = true;
							}
						}
					}

					if (!stopped) {
						model->get_state(toProcess[i]->state, i);
						toProcess[i]->last_token = token;
						toProcess[i]->context.push_back(token);
					}

					if (toProcess[i]->max_queued_tokens == 0) {
						toProcess[i]->busy = false;
					}

					std::cout << "token: " << i << " processed" << std::endl;
				}
			}
		
	};

	Variant createAgent() {
		if (agents.size() < max_agents) {
			Agent *agent = new Agent(model, tokenizer);
			agents.push_back(agent);
			return Variant(agent);
		}

		print_error("max agents reached");
		return Variant();
		
	}
	
	protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("listen"), &GodotRWKV::listen);	
		ClassDB::bind_method(D_METHOD("loadModel"), &GodotRWKV::loadModel);
		ClassDB::bind_method(D_METHOD("loadTokenizer"), &GodotRWKV::loadTokenizer);
		ClassDB::bind_method(D_METHOD("createAgent"), &GodotRWKV::createAgent);
	}
};

// uninclude <vulkan/vulkan.hpp>


#endif // RWKV_GODOT_H
