#!/usr/bin/env python3
"""Recompute the survey's assumptions; no API calls and no live price lookup."""
from decimal import Decimal as D
import json

M = D(1_000_000)
rounds = D(50 * 30)
calls = rounds * D('1.3')
papers = D(2 * 30)  # 40 summaries AND one compilation per issue
cheap = ((rounds * 1000 + 10 * 30 * 3000 + papers * 40 * 3000) * D('.2')
         + (rounds * 30 + 10 * 30 * 300 + papers * 40 * 300) * D('.8')) / M
base = {'asr': rounds * 8 * D('.00033'), 'tts': rounds * 120 / 10000,
        'cheap_excluding_embedding': cheap, 'search': D(10 * 30) * 12 / 1000}
# Tuple: input, hit, output. Missing cache quote uses full input rate.
prices = {
    'qwen3.7-plus': ('2', '2', '8'),
    'doubao-seed-2.0-pro': ('3.2', '.64', '16'),
    'deepseek-v4-pro-offpeak': ('4.5', '.15', '13.5'),
    'deepseek-v4-pro-peak': ('9', '.30', '27'),
    'doubao-seed-2.1-pro': ('6', '1.2', '30'),
    'glm-5.3': ('8', '8', '28'),
    'qwen3.8-max': ('12', '12', '36'),
    'kimi-k3': ('20', '20', '100'),
}

def model_cost(rates, input_tokens=5000, cached_tokens=3000, output_tokens=200):
    inp, hit, out = map(D, rates)
    return ((calls * (input_tokens - cached_tokens) + papers * 10000) * inp
            + calls * cached_tokens * hit
            + (calls * output_tokens + papers * 2000) * out) / M

result = {'assumptions': {'rounds': int(rounds), 'calls': int(calls),
                         'issues': int(papers), 'input_million': str(calls * 5000 / M),
                         'output_million': str(calls * 200 / M)},
          'base': base, 'base_total_excluding_embedding': sum(base.values()),
          'optional_tts': D(38), 'dictation_5_minutes_daily': D(30 * 300) * D('.00033'),
          'models': {}}
for name, rates in prices.items():
    main = model_cost(rates)
    result['models'][name] = {'main': main, 'total_excluding_embedding': main + sum(base.values()),
                             'plus_optional_tts': main + sum(base.values()) + 38,
                             'main_zero_cache_hits': model_cost(rates, cached_tokens=0),
                             'main_10k_input_1000_output_zero_cache': model_cost(rates, 10000, 0, 1000)}
assert model_cost(prices['qwen3.7-plus']) == D('24.78')
assert model_cost(prices['deepseek-v4-pro-peak']) == D('56.025')
assert sum(base.values()) == D('28.164')
result['one_MiB_stop_and_wait_seconds_at_1_6_Mbps'] = {
    str(rtt): D(1024 * 1024 * 8) / 1_600_000 + 64 * D(str(rtt))
    for rtt in (.05, .10, .20)}
result['audio_buffer_seconds'] = D(512) * D('.06')
print(json.dumps(result, indent=2, ensure_ascii=False, default=str))
