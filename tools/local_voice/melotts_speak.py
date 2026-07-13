import argparse
import audioop
import os
import wave
from pathlib import Path


def resolve_language(value: str) -> str:
    normalized = (value or "kr").strip().lower()
    languages = {
        "kr": "KR",
        "ko": "KR",
        "korean": "KR",
        "en": "EN",
        "english": "EN",
        "jp": "JP",
        "ja": "JP",
        "japanese": "JP",
        "zh": "ZH",
        "chinese": "ZH",
        "fr": "FR",
        "french": "FR",
        "es": "ES",
        "spanish": "ES",
    }
    return languages.get(normalized, value.strip().upper())


def read_text(args: argparse.Namespace) -> str:
    if args.text_file:
        return Path(args.text_file).read_text(encoding="utf-8").strip()
    return (args.text or "").strip()


def apply_gain(path: Path, gain: float) -> None:
    if gain <= 0 or abs(gain - 1.0) < 0.001:
        return

    with wave.open(str(path), "rb") as source:
        params = source.getparams()
        frames = source.readframes(source.getnframes())

    louder = audioop.mul(frames, params.sampwidth, gain)
    with wave.open(str(path), "wb") as target:
        target.setparams(params)
        target.writeframes(louder)


def has_cached_files(path: Path) -> bool:
    if not path.exists():
        return False
    return any(item.is_file() for item in path.rglob("*"))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate local ShowDown TTS with MeloTTS.")
    parser.add_argument("--text", default="")
    parser.add_argument("--text-file", default="")
    parser.add_argument("--output", required=True)
    parser.add_argument("--language", default="kr")
    parser.add_argument("--speed", type=float, default=1.0)
    parser.add_argument("--gain", type=float, default=1.0)
    parser.add_argument("--cache-dir", default="")
    parser.add_argument("--device", default="auto")
    parser.add_argument("--allow-download", action="store_true")
    args = parser.parse_args()

    text = read_text(args)
    if not text:
        raise ValueError("No TTS text was provided.")

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if args.cache_dir:
        cache_path = Path(args.cache_dir).resolve()
        cache_dir = str(cache_path)
        os.environ.setdefault("HF_HOME", cache_dir)
        os.environ.setdefault("TRANSFORMERS_CACHE", cache_dir)
        os.environ.setdefault("HUGGINGFACE_HUB_CACHE", str(Path(cache_dir) / "hub"))
        os.environ.setdefault("HF_HUB_DISABLE_SYMLINKS_WARNING", "1")
        if not args.allow_download and has_cached_files(cache_path):
            os.environ.setdefault("HF_HUB_OFFLINE", "1")
            os.environ.setdefault("TRANSFORMERS_OFFLINE", "1")

    from melo.api import TTS

    language = resolve_language(args.language)
    model = TTS(language=language, device=args.device)
    speaker_id = model.hps.data.spk2id[language]
    model.tts_to_file(text, speaker_id, str(output_path), speed=max(0.1, args.speed))
    apply_gain(output_path, max(0.1, args.gain))
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
