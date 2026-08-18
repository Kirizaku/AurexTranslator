# Copyright (C) 2025-2026 by Daniil Nabiulin
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import argparse
import asyncio
import sys

import edge_tts
from flask import Flask, Response, jsonify, request

def synthesize(text, voice, rate):
    async def run():
        audio = bytearray()
        speech = edge_tts.Communicate(text, voice, rate=rate)

        async for chunk in speech.stream():
            if chunk["type"] == "audio":
                audio.extend(chunk["data"])

        return bytes(audio)
    return asyncio.run(run())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--voice", default="", help="voice used when a request names none")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()

    try:
        catalog = asyncio.run(edge_tts.list_voices())
    except Exception as error:
        print("could not read the voice list: %s" % error, file=sys.stderr, flush=True)
        return 1

    voices = {
        voice["ShortName"]: {
            "locale": voice.get("Locale", ""),
            "gender": voice.get("Gender", ""),
        }
        for voice in catalog
    }

    default_voice = args.voice if args.voice in voices else ""

    app = Flask(__name__)

    @app.get("/info")
    def info():
        return jsonify({"name": default_voice, "voices": len(voices)})

    @app.get("/voices")
    def voice_list():
        return jsonify(voices)

    @app.post("/synthesize")
    def speak():
        payload = request.get_json(force=True, silent=True) or {}

        text = (payload.get("text") or "").strip()
        if not text:
            return Response("no text given", status=400)

        voice = payload.get("voice") or default_voice
        if not voice:
            return Response("no voice given and none set at startup", status=400)

        if voice not in voices:
            return Response("unknown voice: %s" % voice, status=400)

        try:
            audio = synthesize(text, voice, payload.get("rate") or "+0%")
        except Exception as error:
            print("synthesis failed: %s" % error, file=sys.stderr, flush=True)
            return Response("synthesis failed: %s" % error, status=500)

        if not audio:
            return Response("the service returned no audio", status=502)

        return Response(audio, mimetype="audio/mpeg")

    print("Serving on %s:%d, %d voice(s)" % (args.host, args.port, len(voices)), flush=True)
    app.run(host=args.host, port=args.port, threaded=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
