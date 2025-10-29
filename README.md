# Protocolo HTTP — Cliente e Servidor em C

Trabalho desenvolvido na disciplina de Redes de Computadores (UFSJ - 2025/2) por **João Lucas de Vilas Bôas Faria**. O projeto implementa um Servidor HTTP simples e um Cliente HTTP, utilizando linguagem C.

---

## Compilação
Para compilar o servidor e o cliente, utilize:
make

Para limpar os arquivos compilados:
make clean

Os executáveis gerados são:
- meu_servidor
- meu_navegador

---

## Cliente HTTP (Parte 1)
O cliente realiza requisições HTTP simples e salva o conteúdo recebido.

Execução:
./meu_navegador http://localhost:5000/index.html

Exemplos:
- ./meu_navegador http://localhost:5000/foto.jpg
- ./meu_navegador http://localhost:5000/arq1.txt
- ./meu_navegador http://localhost:5000/naoexiste.xyz

---

## Servidor HTTP (Parte 2)
O servidor disponibiliza arquivos de um diretório via protocolo HTTP/1.0.

Execução:
./meu_servidor 5000 ./www

Funcionalidades:
- Envia qualquer arquivo dentro do diretório especificado.
- Retorna index.html quando o caminho é /.
- Gera listagem HTML se o diretório não possuir index.html.
- Impede acesso fora do diretório (proteção contra ..).
- Responde com os códigos:
  - 200 OK — sucesso.
  - 404 Not Found — arquivo inexistente.
  - 403 Forbidden — acesso inválido.
  - 405 Method Not Allowed — método diferente de GET.

Exemplo:
./meu_servidor 5000 ./www
(acessar em um navegador: http://localhost:5000/)

---

## Testes Realizados
- Página inicial com index.html e imagem.
- Download de arquivos de texto e imagens.
- Diretório sem index.html listando arquivos.
- Resposta 404 tratada corretamente.
- Cliente salvando apenas quando status 200.

---

## Limitações
- Implementa apenas HTTP/1.0 (sem HTTPS e sem Transfer-Encoding: chunked).
- Não segue redirecionamentos (301/302).
- Aceita apenas o método GET.
