---
type: computation
title: Monthly revenue
runtime: python3.12
parameters:
  - name: month
    type: string
    required: true
computation: /computations/revenue.py
executor:
  resource: /executors/runner.md
  receipt: /receipts/run-42.json
attester:
  resource: /attesters/auditor.md
---

# Monthly revenue

The computation.
