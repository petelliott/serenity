/*
 * Copyright (c) 2021, Peter Elliott <pelliott@ualberta.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ClientConnection.h"
#include <LibGUI/InputBox.h>
#include <LibGUI/MessageBox.h>

namespace KeyServer {

static HashMap<int, RefPtr<ClientConnection>> s_connections;
static Optional<KeyringFile> s_keyring;

ClientConnection::ClientConnection(NonnullRefPtr<Core::LocalSocket> client_socket, int client_id, const StringView& path)
    : IPC::ClientConnection<KeyClientEndpoint, KeyServerEndpoint>(*this, move(client_socket), client_id)
    , m_path(path)
{
    s_connections.set(client_id, *this);
}

ClientConnection::~ClientConnection()
{
}

void ClientConnection::die()
{
    s_connections.remove(client_id());
}

OwnPtr<Messages::KeyServer::GreetResponse> ClientConnection::handle(const Messages::KeyServer::Greet&)
{
    return make<Messages::KeyServer::GreetResponse>();
}

OwnPtr<Messages::KeyServer::AddUsernamePasswordResponse> ClientConnection::handle(const Messages::KeyServer::AddUsernamePassword& message)
{
    auto keyring = get_keyring();
    if (!keyring)
        return make<Messages::KeyServer::AddUsernamePasswordResponse>(false);

    JsonObject entry;
    entry.set("username", message.username());
    entry.set("password", message.password());

    keyring->username_object().set(message.id(), entry);
    keyring->sync();

    return make<Messages::KeyServer::AddUsernamePasswordResponse>(true);
}

OwnPtr<Messages::KeyServer::GetUsernamePasswordResponse> ClientConnection::handle(const Messages::KeyServer::GetUsernamePassword& message)
{
    auto keyring = get_keyring();
    if (!keyring)
        return make<Messages::KeyServer::GetUsernamePasswordResponse>(false, false, "", "");

    auto entry = keyring->username_object().get(message.id());

    if (entry.is_null())
        return make<Messages::KeyServer::GetUsernamePasswordResponse>(true, false, "", "");

    return make<Messages::KeyServer::GetUsernamePasswordResponse>(
        true, true,
        entry.as_object().get("username").as_string(),
        entry.as_object().get("password").as_string());
}

OwnPtr<Messages::KeyServer::AddKeyResponse> ClientConnection::handle(const Messages::KeyServer::AddKey& message)
{
    auto keyring = get_keyring();
    if (!keyring)
        return make<Messages::KeyServer::AddKeyResponse>(false);

    keyring->key_object().set(message.id(), message.key());
    keyring->sync();

    return make<Messages::KeyServer::AddKeyResponse>(true);
}

OwnPtr<Messages::KeyServer::GetKeyResponse> ClientConnection::handle(const Messages::KeyServer::GetKey& message)
{
    auto keyring = get_keyring();
    if (!keyring)
        return make<Messages::KeyServer::GetKeyResponse>(false, false, "");

    auto entry = keyring->key_object().get(message.id());

    if (entry.is_null())
        return make<Messages::KeyServer::GetKeyResponse>(true, false, "");

    return make<Messages::KeyServer::GetKeyResponse>(true, true, entry.as_string());
}

KeyringFile* ClientConnection::get_keyring()
{
    if (s_keyring.has_value())
        return &(s_keyring.value());

    String password;
    // FIXME: Hide the text of the password.
    auto result = GUI::InputBox::show(password, nullptr, "Password", "Enter your keyring password");
    if (result != GUI::InputBox::ExecOK)
        return nullptr;

    s_keyring = KeyringFile::open(m_path, password);
    if (s_keyring.has_value())
        return &(s_keyring.value());

    GUI::MessageBox::show_error(nullptr, "Unable to access or decrypt keyring.");
    return nullptr;
}

}
