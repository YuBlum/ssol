# Simple SOL

Simple SOL, ou Simple Stack-Oriented Language, é uma linguagem de programação procedural baseada em stack. Tem como inspirações Forth, Porth, C e Assembly, é escrito em C.

## Começo Rapido

Em outras linguagens de programação quando você quer somar dois numeros você precisa deixar salvo em uma variavel, ou como argumento de uma função. Por exemplo:

```python
print(10 + 20)
```

Já em Simple SOL você precisa somente colocar os números no stack e dar print neles. Para colocar um número é só digita-lo. Por exemplo:

```ssol
10 20 + print
```

O programa acima coloca o número 10 no stack, depois o número 20, remove eles do stack, coloca o resultado da soma e da print no número.
