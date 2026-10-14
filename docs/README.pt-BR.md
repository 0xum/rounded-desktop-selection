# Rounded Desktop Selection

Mod Windhawk para deixar o retângulo de seleção por arraste do desktop
translúcido e arredondado, com cor, opacidade, raio e borda configuráveis.
Versão **0.1.2**, MVP experimental para Windows 11 x64.

![Prévia gerada pelos testes de desenho](preview.png)

Padrão: vermelho `#E81123`, preenchimento com 20% de opacidade, borda com 80%,
cantos de 8 DIP e borda de 1 DIP. O Windows mantém a seleção retangular nativa.
Este MVP atua no desktop; a lista de arquivos do Explorador moderno não é coberta.

## Instalação

No Windhawk, escolha **Create a New Mod**, substitua todo o conteúdo de
`mod.wh.cpp` pelo arquivo `rounded-desktop-selection.wh.cpp` deste repositório,
compile pela barra lateral e habilite o mod. Ajuste as opções em Settings.

Para compilar pelos scripts, com Windhawk instalado:

```powershell
.\scripts\check-project.ps1
.\build.ps1
.\build.ps1 -Test
```

Para instalar, abra PowerShell como administrador e execute `install.ps1`.
Para uma instalação local existente, use `update.ps1`; ele preserva as
configurações e o estado de habilitação. Os scripts de instalação não atendem
ao modo portátil do Windhawk; nesse caso, use o editor.

Desabilite pelo Windhawk ou execute `disable.ps1` como administrador para
retornar ao desenho nativo. `enable.ps1` habilita novamente.

## Compatibilidade e configuração

Validado com Windhawk 1.7.3, Windows 11 build 26300 e comctl32 6.0.26100.9278.
Outras builds, ARM64, Windows 10 e múltiplas escalas de monitor ainda precisam
de validação. O mod mantém a pintura nativa quando não reconhece o layout interno.

O teste inicial confirmou seleção sem rastros. Se o arraste falhar com o
Lively Wallpaper ativo, compare o funcionamento nativo com o mod desabilitado.
No ambiente usado na validação, fechar o Lively restaurou a seleção nativa.

Consulte [configuração](configuration.md), [desenvolvimento](development.md)
e o [README completo](../README.md). Arquivos `*.local.json`, binários e logs
ficam fora do Git. Licença [MIT](../LICENSE).
