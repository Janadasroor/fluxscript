/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

#ifndef FLUX_NATURAL_LANGUAGE_H
#define FLUX_NATURAL_LANGUAGE_H

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {
namespace NaturalLanguage {

// ============================================================================
// NLP Data Structures
// ============================================================================

enum class Intent
{
    Query,    // "What is the gain?"
    Command,  // "Run simulation"
    Modify,   // "Change R1 to 10k"
    Debug,    // "Why is the output clipped?"
    Explain,  // "Explain this circuit"
    Compare,  // "Compare v1 and v2"
    Generate, // "Generate a testbench"
    Unknown
};

struct ParsedQuery
{
    Intent intent;
    std::string action;
    std::vector<std::string> entities;
    std::map<std::string, std::string> parameters;
    std::string target; // Circuit, component, or net
    double confidence;

    ParsedQuery() : intent(Intent::Unknown), confidence(0) {}
};

struct QueryResponse
{
    bool success;
    std::string text;
    std::string type; // "value", "plot", "table", "explanation"
    std::map<std::string, double> data;
    std::vector<std::string> suggestions;

    QueryResponse() : success(false) {}
};

// ============================================================================
// Natural Language Processor
// ============================================================================

class NLPProcessor
{
public:
    NLPProcessor();
    ~NLPProcessor();

    // Initialize with domain knowledge
    void initialize(const std::string& domain = "electronics");

    // Parse natural language
    ParsedQuery parse(const std::string& query);

    // Execute parsed query
    QueryResponse execute(const ParsedQuery& query);

    // Direct query interface
    QueryResponse query(const std::string& text);

    // Context management
    void setContext(const std::string& key, const std::string& value);
    std::string getContext(const std::string& key) const;
    void clearContext();

    // Learning
    void train(const std::string& query, const std::string& expectedIntent);
    void addSynonym(const std::string& word, const std::string& synonym);

private:
    std::map<std::string, std::string> m_context;
    std::map<std::string, std::vector<std::string>> m_synonyms;
    std::map<std::string, Intent> m_intentPatterns;

    // NLP methods
    std::vector<std::string> tokenize(const std::string& text);
    std::string lemmatize(const std::string& word);
    Intent classifyIntent(const std::vector<std::string>& tokens);
    std::vector<std::string> extractEntities(const std::vector<std::string>& tokens);
};

// ============================================================================
// Circuit Explainer
// ============================================================================

class CircuitExplainer
{
public:
    CircuitExplainer();
    ~CircuitExplainer();

    // Load circuit
    void loadCircuit(const std::string& circuitFile);

    // Generate explanations
    std::string explainFunction();
    std::string explainOperation();
    std::string explainComponent(const std::string& component);
    std::string explainNet(const std::string& net);

    // Answer questions
    std::string answerWhy(const std::string& question);
    std::string answerHow(const std::string& question);
    std::string answerWhat(const std::string& question);

    // Generate documentation
    std::string generateDocumentation();
    std::string generateBOM();
    std::string generateTheoryOfOperation();

    // Complexity analysis
    struct ComplexityReport
    {
        int numComponents;
        int numNets;
        int numStages;
        std::string complexityLevel; // "simple", "moderate", "complex"
        std::vector<std::string> keyComponents;
        std::vector<std::string> criticalNets;
    };

    ComplexityReport analyzeComplexity();

private:
    std::string m_circuitFile;
    std::map<std::string, std::string> m_componentFunctions;
};

// ============================================================================
// Voice Interface
// ============================================================================

class VoiceInterface
{
public:
    static VoiceInterface& instance();

    // Initialize
    void initialize();

    // Speech recognition
    std::string recognize(const std::vector<double>& audioData);

    // Text-to-speech
    std::vector<double> synthesize(const std::string& text);

    // Voice commands
    void registerCommand(const std::string& phrase, const std::string& action);
    void executeVoiceCommand(const std::string& command);

    // Configuration
    void setLanguage(const std::string& language);
    void setVoice(const std::string& voice);
    void setSpeed(double speed);

private:
    VoiceInterface();
    std::map<std::string, std::string> m_commands;
    std::string m_language;
};

// ============================================================================
// Conversational Assistant
// ============================================================================

class ConversationalAssistant
{
public:
    ConversationalAssistant();
    ~ConversationalAssistant();

    // Start conversation
    void startSession();
    std::string processMessage(const std::string& message);
    void endSession();

    // Personality
    void setPersonality(const std::string& personality); // "professional", "friendly", "concise"

    // Memory
    void remember(const std::string& fact);
    std::string recall(const std::string& topic);

    // Proactive suggestions
    std::vector<std::string> getSuggestions();

private:
    bool m_sessionActive;
    std::string m_personality;
    std::vector<std::string> m_memory;
    NLPProcessor m_nlp;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
// NLP Processor
void* flux_nlp_create();
void flux_nlp_destroy(void* nlp);
void flux_nlp_initialize(void* nlp, const char* domain);
const char* flux_nlp_query(void* nlp, const char* query);

// Circuit Explainer
void* flux_explainer_create();
void flux_explainer_destroy(void* explainer);
void flux_explainer_load(void* explainer, const char* circuitFile);
const char* flux_explainer_explain(void* explainer);
const char* flux_explainer_answer(void* explainer, const char* question);

// Voice Interface
void* flux_voice_create();
void flux_voice_destroy(void* voice);
void flux_voice_initialize(void* voice);
const char* flux_voice_recognize(void* voice, const double* audio, int length);
void flux_voice_speak(void* voice, const char* text);

// Conversational Assistant
void* flux_assistant_create();
void flux_assistant_destroy(void* assistant);
const char* flux_assistant_message(void* assistant, const char* message);
}

} // namespace NaturalLanguage
} // namespace Flux

#endif // FLUX_NATURAL_LANGUAGE_H
