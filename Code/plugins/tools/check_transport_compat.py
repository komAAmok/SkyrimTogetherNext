#!/usr/bin/env python3
"""Keep the documented plugin-transport compatibility claims true.

The companion-plugin documentation states facts that are cheap to break and
expensive to notice: that the two plugin opcodes were appended rather than
inserted, that they sit at or above the previous maximum, and that the chat
opcodes the standalone bridge matches on keep their indices. Those are the
claims a reader relies on when reasoning about a mixed-version session, so they
are checked against the source instead of trusted.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
OPCODES_REL = 'Code/encoding/Opcodes.h'

# The standalone STRPM bridge matches these two indices in the official 1.8.0
# build; this fork has to keep the same numbering for the fallback to work.
CHAT_SEND_INDEX = 38
CHAT_BROADCAST_INDEX = 36


def members(text: str, enum_name: str) -> list[str]:
    body = re.search(r'enum ' + enum_name + r'[^{]*\{(.*?)\n\};', text, re.S).group(1)
    out = []
    for line in body.splitlines():
        line = line.split('//')[0].strip()
        if not line:
            continue
        out.append(line.rstrip(',').split('=')[0].strip())
    return out


text = (ROOT / OPCODES_REL).read_text(encoding='utf-8')
client = members(text, 'ClientOpcode')
server = members(text, 'ServerOpcode')

failures: list[str] = []


def index_of(names: list[str], member: str) -> int:
    if member not in names:
        failures.append(f'{member} is missing from {OPCODES_REL}')
        return -1
    return names.index(member)


send_index = index_of(client, 'kSendChatMessageRequest')
broadcast_index = index_of(server, 'kNotifyChatMessageBroadcast')
request_index = index_of(client, 'kPluginMessagingRequest')
notify_index = index_of(server, 'kNotifyPluginMessaging')

if send_index >= 0 and send_index != CHAT_SEND_INDEX:
    failures.append(
        f'kSendChatMessageRequest moved to index {send_index}; the standalone bridge matches '
        f'{CHAT_SEND_INDEX} and would stop working. Appending is fine, inserting is not.'
    )

if broadcast_index >= 0 and broadcast_index != CHAT_BROADCAST_INDEX:
    failures.append(
        f'kNotifyChatMessageBroadcast moved to index {broadcast_index}; the standalone bridge '
        f'matches {CHAT_BROADCAST_INDEX} and would stop working.'
    )

# An older peer rejects any opcode at or above its own maximum, so the new
# opcodes must not sit below the pre-existing ones.
if request_index >= 0 and 'kSetTimeCommandRequest' in client:
    if request_index <= client.index('kSetTimeCommandRequest'):
        failures.append('kPluginMessagingRequest was inserted before existing client opcodes')

if notify_index >= 0 and 'kNotifySetTimeResult' in server:
    if notify_index <= server.index('kNotifySetTimeResult'):
        failures.append('kNotifyPluginMessaging was inserted before existing server opcodes')

print(f'client opcodes: {len(client) - 1} in use, plugin request at index {request_index}')
print(f'server opcodes: {len(server) - 1} in use, plugin notify at index {notify_index}')
print(f'chat opcodes  : send {send_index}, broadcast {broadcast_index}')

if failures:
    print()
    print(f'TRANSPORT COMPATIBILITY FAILED ({len(failures)})')
    for failure in failures:
        print('  -', failure)
    sys.exit(1)

print()
print('TRANSPORT COMPATIBILITY OK')
print('  new opcodes appended, chat indices unchanged, docs remain accurate')