# User Settings & Constants
# Generated from ユーザー設定.md

AGENT_SYSTEM_PROMPT = {
    "default": "あなたはMacに最適化された有能なアシスタントです。",
    "node_analysis": "以下のコードの依存関係を解析し、JSON出力せよ。"
}

MODEL_CONFIG = {
    "DEFAULT_MODEL_PATH": "models/qwen2.5-7b",
    "MAX_TOKENS": 2048,
    "TEMPERATURE": 0.7
}

UI_CONFIG = {
    "RETINA_SCALE": 2.0,
    "WINDOW_WIDTH": 1280,
    "WINDOW_HEIGHT": 800,
    "THEME": "Professional Dark",
    "FONT_PATH": "resources/fonts/NotoSansJP-Regular.otf"
}

PERFORMANCE_CONFIG = {
    "ENABLE_AST_CACHE": True,
    "CACHE_LIMIT_GB": 16.0,
    "THREAD_POOL_SIZE": 8,
    "PYTHON_ASYNC_QUEUE_SIZE": 100
}
