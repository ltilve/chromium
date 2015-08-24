// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/i18n_custom_bindings.h"

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/message_bundle.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/cld_2/src/public/compact_lang_det.h"
#include "third_party/cld_2/src/public/encodings.h"

namespace extensions {

using namespace v8_helpers;

namespace {

// Max number of languages detected by CLD2.
const int kCldNumLangs = 3;

struct DetectedLanguage {
  DetectedLanguage(const std::string& language, int percentage)
      : language(language), percentage(percentage) {}
  ~DetectedLanguage() {}

  // Returns a new v8::Local<v8::Value> representing the serialized form of
  // this DetectedLanguage object.
  v8::Local<v8::Value> ToValue(ScriptContext* context);

  std::string language;
  int percentage;

 private:
  DISALLOW_COPY_AND_ASSIGN(DetectedLanguage);
};

// LanguageDetectionResult object that holds detected langugae reliability and
// array of DetectedLanguage
struct LanguageDetectionResult {
  explicit LanguageDetectionResult(bool is_reliable)
      : is_reliable(is_reliable) {}
  ~LanguageDetectionResult() {}

  // Returns a new v8::Local<v8::Value> representing the serialized form of
  // this Result object.
  v8::Local<v8::Value> ToValue(ScriptContext* context);

  // CLD detected language reliability
  bool is_reliable;

  // Array of detectedLanguage of size 1-3. The null is returned if
  // there were no languages detected
  ScopedVector<DetectedLanguage> languages;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageDetectionResult);
};

v8::Local<v8::Value> DetectedLanguage::ToValue(ScriptContext* context) {
  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Isolate* isolate = v8_context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::Object> value(v8::Object::New(isolate));
  SetProperty(v8_context, value, ToV8StringUnsafe(isolate, "language"),
              ToV8StringUnsafe(isolate, language.c_str()));

  SetProperty(v8_context, value, ToV8StringUnsafe(isolate, "percentage"),
              v8::Number::New(isolate, percentage));

  v8::Local<v8::Value> result = v8::Local<v8::Value>::Cast(value);
  return handle_scope.Escape(result);
}

v8::Local<v8::Value> LanguageDetectionResult::ToValue(ScriptContext* context) {
  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Isolate* isolate = v8_context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::Object> value(v8::Object::New(isolate));
  SetProperty(v8_context, value, ToV8StringUnsafe(isolate, "isReliable"),
              v8::Boolean::New(isolate, is_reliable));

  v8::Local<v8::Array> langs(v8::Array::New(isolate, languages.size()));
  for (size_t i = 0; i < languages.size(); ++i) {
    SetProperty(v8_context, langs, i, languages[i]->ToValue(context));
  }

  SetProperty(isolate->GetCurrentContext(), value,
              ToV8StringUnsafe(isolate, "languages"), langs);

  v8::Local<v8::Value> result = v8::Local<v8::Value>::Cast(value);
  return handle_scope.Escape(result);
}

void InitDetectedLanguages(CLD2::Language* languages,
                           int* percents,
                           ScopedVector<DetectedLanguage>* detected_languages) {
  for (int i = 0; i < kCldNumLangs; i++) {
    std::string language_code;
    // Convert LanguageCode 'zh' to 'zh-CN' and 'zh-Hant' to 'zh-TW' for
    // Translate server usage. see DetermineTextLanguage in
    // components/translate/core/language_detection/language_detection_util.cc
    if (languages[i] == CLD2::UNKNOWN_LANGUAGE) {
      // Break from the loop since there is no need to save
      // unknown languages
      break;
    } else {
      language_code =
          CLD2::LanguageCode(static_cast<CLD2::Language>(languages[i]));
    }
    detected_languages->push_back(
        new DetectedLanguage(language_code, percents[i]));
  }
}

}  // namespace

I18NCustomBindings::I18NCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetL10nMessage",
      base::Bind(&I18NCustomBindings::GetL10nMessage, base::Unretained(this)));
  RouteFunction("GetL10nUILanguage",
                base::Bind(&I18NCustomBindings::GetL10nUILanguage,
                           base::Unretained(this)));
  RouteFunction("DetectTextLanguage",
                base::Bind(&I18NCustomBindings::DetectTextLanguage,
                           base::Unretained(this)));
}

void I18NCustomBindings::GetL10nMessage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsString()) {
    NOTREACHED() << "Bad arguments";
    return;
  }

  std::string extension_id;
  if (args[2]->IsNull() || !args[2]->IsString()) {
    return;
  } else {
    extension_id = *v8::String::Utf8Value(args[2]);
    if (extension_id.empty())
      return;
  }

  L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    content::RenderFrame* render_frame = context()->GetRenderFrame();
    if (!render_frame)
      return;

    L10nMessagesMap messages;
    // A sync call to load message catalogs for current extension.
    render_frame->Send(
        new ExtensionHostMsg_GetMessageBundle(extension_id, &messages));

    // Save messages we got.
    ExtensionToL10nMessagesMap& l10n_messages_map =
        *GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = GetL10nMessagesMap(extension_id);
  }

  std::string message_name = *v8::String::Utf8Value(args[0]);
  std::string message =
      MessageBundle::GetL10nMessage(message_name, *l10n_messages);

  v8::Isolate* isolate = args.GetIsolate();
  std::vector<std::string> substitutions;
  if (args[1]->IsArray()) {
    // chrome.i18n.getMessage("message_name", ["more", "params"]);
    v8::Local<v8::Array> placeholders = v8::Local<v8::Array>::Cast(args[1]);
    uint32_t count = placeholders->Length();
    if (count > 9)
      return;
    for (uint32_t i = 0; i < count; ++i) {
      substitutions.push_back(*v8::String::Utf8Value(placeholders->Get(
          v8::Integer::New(isolate, i))));
    }
  } else if (args[1]->IsString()) {
    // chrome.i18n.getMessage("message_name", "one param");
    substitutions.push_back(*v8::String::Utf8Value(args[1]));
  }

  args.GetReturnValue().Set(v8::String::NewFromUtf8(
      isolate,
      base::ReplaceStringPlaceholders(message, substitutions, NULL).c_str()));
}

void I18NCustomBindings::GetL10nUILanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8::String::NewFromUtf8(
      args.GetIsolate(), content::RenderThread::Get()->GetLocale().c_str()));
}

void I18NCustomBindings::DetectTextLanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsString());

  std::string text = *v8::String::Utf8Value(args[0]);
  CLD2::CLDHints cldhints = {nullptr, "", CLD2::UNKNOWN_ENCODING,
                             CLD2::UNKNOWN_LANGUAGE};

  bool is_plain_text = true;  // assume the text is a plain text
  int flags = 0;              // no flags, see compact_lang_det.h for details
  int text_bytes;             // amount of non-tag/letters-only text (assumed 0)
  int valid_prefix_bytes;     // amount of valid UTF8 character in the string
  double normalized_score[kCldNumLangs];

  CLD2::Language languages[kCldNumLangs];
  int percents[kCldNumLangs];
  bool is_reliable = false;

  // populating languages and percents
  int cld_language = CLD2::ExtDetectLanguageSummaryCheckUTF8(
      text.c_str(), static_cast<int>(text.size()), is_plain_text, &cldhints,
      flags, languages, percents, normalized_score,
      nullptr,  // assumed no ResultChunkVector is used
      &text_bytes, &is_reliable, &valid_prefix_bytes);

  // Check if non-UTF8 character is encountered
  // See bug http://crbug.com/444258.
  if (valid_prefix_bytes < static_cast<int>(text.size()) &&
      cld_language == CLD2::UNKNOWN_LANGUAGE) {
    // Detect Language upto before the first non-UTF8 character
    CLD2::ExtDetectLanguageSummary(
        text.c_str(), valid_prefix_bytes, is_plain_text, &cldhints, flags,
        languages, percents, normalized_score,
        nullptr,  // assumed no ResultChunkVector is used
        &text_bytes, &is_reliable);
  }

  LanguageDetectionResult result(is_reliable);
  // populate LanguageDetectionResult with languages and percents
  InitDetectedLanguages(languages, percents, &result.languages);

  args.GetReturnValue().Set(result.ToValue(context()));
}

}  // namespace extensions
