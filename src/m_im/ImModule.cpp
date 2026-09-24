/**
 * Copyright (C) 2021-2024 Saturneric <eric@bktus.com>
 *
 * This file is part of GpgFrontend.
 *
 * GpgFrontend is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GpgFrontend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GpgFrontend. If not, see <https://www.gnu.org/licenses/>.
 *
 * The initial version of the source code is inherited from
 * the gpg4usb project, which is under GPL-3.0-or-later.
 *
 * All the source code of GpgFrontend was modified and released by
 * Saturneric <eric@bktus.com> starting on May 12, 2021.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <GFModule.h>
#include <GFSDK.hpp>
#include <GFSDKHostCommands.hpp>

#include <QtWidgets>

#include "GFModuleIdentity.h"
#include "ImCodec.h"
#include "ImSettingsPage.h"

/**
 * @file ImModule.cpp
 * @brief Instant messaging: OpenPGP messages as single Base58 tokens.
 *
 * The module owns the token format, the book phrase and the page to set it.
 * The Host keeps everything else: key selection, the OpenPGP operations, the
 * editor and the result display. The two meet at two codecs:
 *
 *  - `.decode` is offered every text the user decrypts, before the Host's own
 *    decrypt. It claims only its own tokens, so a normal Decrypt of a token
 *    just works, and anything else is decrypted as before.
 *  - `.encode` turns the Host's binary encrypt output into a token, when the
 *    user asks for IM Encrypt.
 *
 * Both run on the module runner, never the GUI thread: no dialog, no network.
 */

namespace {

namespace host = gf::cmd::host;
using Severity = host::AppMessage::Severity;

/// Whether to warn before encrypting with the default book. Its own setting,
/// read in the group the Host used to keep it in.
constexpr auto kWarnDefaultBookKey = "warn_default_book";

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("GTrC", text);
}

auto ToResult(const ImCodec::Answer& a) -> host::CodecResult {
  host::CodecResult r;
  r.outcome = static_cast<host::CodecOutcome>(a.outcome);
  if (a.outcome == ImCodec::Outcome::kHandled) {
    r.output = gf::cmd::MakeBlob(a.output.constData(),
                                 static_cast<size_t>(a.output.size()));
  }
  r.error = a.error;
  r.cards = a.cards;
  return r;
}

auto BytesOf(const gf::cmd::Blob& blob) -> QByteArray {
  return {blob.Data(), static_cast<qsizetype>(blob.Size())};
}

struct Decode {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".decode", GC_TR("Instant Message Token"),
      GC_TR("Recognise and unwrap instant messaging tokens before decrypting"),
      "", 0, gf::cmd::kInputDecoder};
  using Args = host::CodecArgs;
  using Result = host::CodecResult;
};

struct Encode {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".encode", GC_TR("Instant Message Token"),
      GC_TR("Wrap an encrypted message as an instant messaging token"), "", 0,
      gf::cmd::kOutputEncoder};
  using Args = host::CodecArgs;
  using Result = host::CodecResult;
};

auto DoDecode(const gf::cmd::CommandContext&, const host::CodecArgs& a)
    -> gf::cmd::Outcome<host::CodecResult> {
  return gf::cmd::Outcome<host::CodecResult>::Success(
      ToResult(ImCodec::DecodeInput(BytesOf(a.input), ImBookPhrase())));
}

auto DoEncode(const gf::cmd::CommandContext&, const host::CodecArgs& a)
    -> gf::cmd::Outcome<host::CodecResult> {
  return gf::cmd::Outcome<host::CodecResult>::Success(
      ToResult(ImCodec::EncodeOutput(BytesOf(a.input), ImBookPhrase())));
}

/**
 * @brief Ask before encrypting with the default book, unless told not to.
 *
 * @return whether to go on
 */
auto ConfirmDefaultBook() -> bool {
  if (!ImBookPhrase().isEmpty()) return true;
  auto* ctx = GFModuleSdkContext();
  if (!gf::sdk::Setting(ctx, GF_SETTING_MODULE, kWarnDefaultBookKey, true)
           .toBool()) {
    return true;
  }

  QMessageBox box;
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(Tr("No Message Book Phrase Set"));
  box.setText(Tr("You have not set a Message Book phrase."));
  box.setInformativeText(Tr(
      "Instant messages are hidden using a shared \"Message Book\". "
      "Without a phrase, GpgFrontend falls back to the built-in default "
      "book and that book ships in every copy of the program. It hides "
      "the format from a simple scanner, but anyone who knows GpgFrontend "
      "can still recognise your message for what it is.\n\n"
      "Your message is OpenPGP-encrypted either way; what is at stake here "
      "is only whether it is recognisable as an encrypted message at all.\n\n"
      "To get that, set a phrase and share it privately with the person you "
      "are writing to. You must both use exactly the same one."));

  auto* settings_button =
      box.addButton(Tr("Open Settings..."), QMessageBox::ActionRole);
  auto* continue_button =
      box.addButton(Tr("Continue with Default"), QMessageBox::AcceptRole);
  auto* never_button =
      box.addButton(Tr("Continue, Don't Ask Again"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(settings_button);
  box.exec();

  auto* clicked = box.clickedButton();
  if (clicked == settings_button) {
    // The settings dialog is not modal, so it cannot be waited on from here:
    // open it and let the user run the operation again.
    Commands().Invoke<host::AppOpenSettings>({});
    return false;
  }
  if (clicked == never_button) {
    gf::sdk::SetSetting(ctx, GF_SETTING_MODULE, kWarnDefaultBookKey, false);
  }
  return clicked == continue_button || clicked == never_button;
}

/// Both IM encrypt commands act on a document, named by the Host's context.
struct TargetArgs {
  gf::cmd::DocumentRef target;
  static constexpr auto Fields() {
    return std::make_tuple(gf::cmd::F("target", &TargetArgs::target));
  }
};

struct EncryptIm {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".encrypt", GC_TR("IM Encrypt"),
      GC_TR("Encrypt the message as a single-line token for chat apps"), "",
      GF_HOST_CAP_EDITOR, gf::cmd::kNeedsGuiThread};
  using Args = TargetArgs;
  using Result = gf::cmd::Unit;
  static auto State(const gf::cmd::CommandContext&) -> uint32_t;
};

struct EncryptSignIm {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".encrypt_sign", GC_TR("IM Encrypt && Sign"),
      GC_TR("Encrypt and sign the message as a single-line token for chat "
            "apps"),
      "", GF_HOST_CAP_EDITOR, gf::cmd::kNeedsGuiThread};
  using Args = TargetArgs;
  using Result = gf::cmd::Unit;
  static auto State(const gf::cmd::CommandContext&) -> uint32_t;
};

/**
 * @brief Enabled exactly when the Host's own operation is: IM Encrypt whenever
 *        the Host could encrypt at all (to the checked keys, or with a
 *        passphrase when none are checked), IM Encrypt & Sign only when its
 *        Encrypt & Sign could -- which needs checked keys, since signing has
 *        no passphrase fallback.
 *
 * Asked each time the menu is shown, never while the module loads: the Host's
 * commands are registered with its window, after modules start.
 */
auto HostState(const char* host_command) -> uint32_t {
  uint32_t bits = 0;
  if (GFCommandQueryState(GFModuleSdkContext(), host_command, &bits) != 0) {
    return GF_CMD_STATE_VISIBLE;
  }
  return (bits & GF_CMD_STATE_ENABLED) != 0
             ? GF_CMD_STATE_ENABLED | GF_CMD_STATE_VISIBLE
             : GF_CMD_STATE_VISIBLE;
}

template <bool kSign>
auto DoEncryptIm(const gf::cmd::CommandContext&, const TargetArgs& a)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  // Ask before the Host makes the user pick recipients, not after.
  if (!ConfirmDefaultBook()) {
    return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
  }
  Commands().Invoke<host::CryptoEncryptEncoded>(
      {a.target, QStringLiteral(GF_MODULE_ID ".encode"), kSign});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

auto EncryptIm::State(const gf::cmd::CommandContext&) -> uint32_t {
  return HostState(host::CryptoEncryptEncoded::kMeta.id);
}

auto EncryptSignIm::State(const gf::cmd::CommandContext&) -> uint32_t {
  return HostState(host::CryptoEncryptSign::kMeta.id);
}

auto OnActivate() -> GFResult {
  // Registered untranslated: the Host translates presentation when shown.
  const bool settings = gf::ui::RegisterNativeWidget<ImSettingsPage>(
      "settings",
      {GC_TR("Instant Messaging"),
       GC_TR("instant messaging,message book,phrase,fingerprint,token"), "",
       "", "", 0, 0},
      [](const QCborMap& /*args*/) { return new ImSettingsPage(); });
  return settings ? GFResult::Ok()
                  : GFResult::Fail("the settings page was not registered");
}

const std::array<gf::cmd::Binding, 4> kCommands = {
    gf::cmd::Bind<Decode, &DoDecode>(),
    gf::cmd::Bind<Encode, &DoEncode>(),
    gf::cmd::Bind<EncryptIm, &DoEncryptIm<false>>(),
    gf::cmd::Bind<EncryptSignIm, &DoEncryptIm<true>>(),
};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // nothing runs between calls; the Host withdraws the rest
    nullptr,
    nullptr,  // no events
    0,
    kCommands.data(),
    kCommands.size(),
};

}  // namespace

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi * {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
