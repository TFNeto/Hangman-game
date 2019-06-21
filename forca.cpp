#include <iostream> // cout
#include <stdlib.h> // exit
#include <string.h> // bzero
#include <unistd.h> // close
#include <sstream>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <set>
#include <postgresql/libpq-fe.h>
#include <map>
#include <string>
#include <vector>


using namespace std;


PGconn* conn = NULL;

vector<int> player;
int erros=0;
string w;
string wstat;
string cop;
vector<string> list;
int turn;



string intToString (int a)
{
    ostringstream temp;
    temp<<a;
    return temp.str();
}
int stringToInt(string s) {
  int i;
  istringstream iss(s);
  iss >> i;
  return i;
}
void turns(){

  for(int n=0; n<(player.size());n++){

    if(turn==player[n]){
    	if(n==(player.size()-1)){
	        turn=player[0];
	        return;
      	}
      	else{
	        turn=player[n+1];
	        return;
      	}
    }

  }

  return;
}

pthread_mutex_t lock;

set<int> clients; // a set of ints with the client socket ids
map<string, int> sockets;
map<int, string> users;
/// funçoes base de dados ///

void dbconn(){
conn = PQconnectdb("host='dbm.fe.up.pt' user='sinf17a41' password='deuspaco'");

if(((!conn) && PQstatus (conn))!=CONNECTION_OK){
	cout<<"Failed to connect to Database: "<< PQerrorMessage(conn)<<endl;
	exit(-1);
}
}
void dbconnend(){

	PQfinish(conn);
}
PGresult* doquery (string query){

  PGresult* res = PQexec(conn, query.c_str());

  if ((PQresultStatus(res) != PGRES_COMMAND_OK) && (PQresultStatus(res) != PGRES_TUPLES_OK)){
    cout <<"erro"<<PQresultErrorMessage(res) << endl;
    return NULL;
 }

  return res;
}

//funcoes sockets
bool writeline(int socketfd, string line) {
  // this code should be improved as the complete line
  // might not be sent in one time
	string tosend = line + "\n";
	write(socketfd, tosend.c_str(), tosend.length());
}

void broadcast(int originfd, string line) {
  set<int>::iterator it;

  for (it = clients.begin(); it != clients.end(); it++) {
    int socketfd = *it;
	    if (socketfd != originfd)
	      writeline (socketfd, line);
  }
}

bool readline(int socketfd, string &line) {
  int n;
  /* buffer with 1025 positions so we have space
     for the string ending \0 character*/
  char buffer[1025];

  /* initialized the string */
  line = "";

  /* While we don't reach the end of the string
     keep reading */
  while (line.find('\n') == string::npos) {
    // n haracters were read. if == 0 we reached the end of the string
    int n = read(socketfd, buffer, 1024);
    if (n == 0) return false;
    buffer[n] = 0; // put a \0 in the end of the buffer
    line += buffer; // add the read text to the string
  }

  // Remove the \r\n
  line.erase(line.end() - 1);
  line.erase(line.end() - 1);
  return true;
}

bool online(string username){
    if (sockets.find(username) != sockets.end())
        return true;
    else
        return false;
}


// funcoes clientes

void registuser (int socket, string line){

  pthread_mutex_lock(&lock);

  string command;
  string username;
  string password;
  string admincode;

  istringstream stream(line);

  stream >> command >> username >> password >> admincode;

  if (username == "" || password == ""){
	writeline(socket,"Erro! Username ou password incompletos!\n");
	pthread_mutex_unlock(&lock);
    return;
  }

  PGresult* res=doquery ("SELECT username FROM users WHERE username='"+username+"'");

  if ((PQntuples(res) != 0 )){
	writeline(socket,"Username já existente, por favor escolha um novo \n");
	pthread_mutex_unlock(&lock);
	return;
  }

  else if((PQntuples(res) == 0 )&& (admincode != "gnomo" )){

	   	doquery("INSERT INTO users VALUES( DEFAULT,'"+username+"','"+password+"',false);");
		writeline(socket, "");
	    writeline(socket,"Utilizador criado com sucesso!");
		pthread_mutex_unlock(&lock);

  }
  else if((PQntuples(res) == 0) && (admincode == "gnomo")){
	  doquery("INSERT INTO users VALUES( DEFAULT,'"+username+"','"+password+"',true);");
	  writeline(socket,"");
	  writeline(socket,"Admin criado com sucesso!");
	  pthread_mutex_unlock(&lock);
}
return;
}

void login(int socket,string line){

  pthread_mutex_lock(&lock);

  string command;
  string username;
  string password;
  string admincode;
  string aux=line;

  istringstream stream(line);

  stream >> command >> username >> password >> admincode;

  if (username == "" || password == ""){
	writeline (socket,"Erro! Username ou password incompletos!\n");
	pthread_mutex_unlock(&lock);
    return;
  }

 if (online(username) == true){
    writeline (socket, "Jogador ja online");
	writeline (socket, "");
    pthread_mutex_unlock(&lock);
    return;
  }
   PGresult* res = doquery ("SELECT username FROM users WHERE username = '"+username+"';");

	if((PQntuples(res) == 0)){
		pthread_mutex_unlock(&lock);
	 	registuser(socket,aux);
		pthread_mutex_lock(&lock);
		res = doquery ("SELECT username FROM users WHERE username = '"+username+"';");
	}
 	if(PQntuples(res) != 0){
		PGresult* res1 = doquery ("SELECT username FROM users WHERE username = '"+username+"'AND pass = '"+password+"' AND admin=false;");
		PGresult* res2 = doquery ("SELECT username FROM users WHERE username = '"+username+"'AND pass = '"+password+"' AND admin=true;");

		if(PQntuples(res1) != 0){
			sockets[username] = socket;
		    	users[socket] = username;
			writeline (socket, "");
			writeline (socket, "Login com sucesso!");
			pthread_mutex_unlock(&lock);
			return;
		}
		if(PQntuples(res2) != 0){
			sockets[username] = socket;
	    	users[socket] = username;
			writeline (socket, "");
			writeline (socket, "Login de administrador com sucesso!");
			pthread_mutex_unlock(&lock);
			return;
		}
		else{
			writeline (socket, "");
			writeline (socket, "Password incorreta, tente novamente!");
			pthread_mutex_unlock(&lock);
			return;
		}
   }
}

void cat(int socket,string line){

pthread_mutex_lock(&lock);

string command;
string category;

istringstream stream(line);

 stream >> command >> category;
 	if (category == ""){
		writeline (socket,"Erro! Nenhuma categoria inserida! \n");
		pthread_mutex_unlock(&lock);
  		return;
 	 }


PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ users[socket] +"'  AND admin= TRUE");
	if(PQntuples(res) == 0){
			writeline(socket, "Desculpa esta função é apenas para administradores.");
	    	writeline(socket, "");
			pthread_mutex_unlock(&lock);
			return;
	}
PGresult* res2 = doquery("SELECT cat FROM category WHERE cat='"+category+"'");
	if(PQntuples(res2) == 0){
		doquery("INSERT INTO category VALUES( DEFAULT,'"+category+"');");
		writeline (socket, "");
		writeline (socket, "Categoria criada com sucesso!");
		pthread_mutex_unlock(&lock);
		return;
	}
	else{
		writeline (socket, "");
		writeline (socket, "Categoria já existente!");
		pthread_mutex_unlock(&lock);
		return;
	}
}

void add(int socket,string line){

pthread_mutex_lock(&lock);

string command;
string word;
string category;
string aux=line;

istringstream stream(line);

 stream >> command >> word >>category;
	if ((category == "")||(word== "")){
		writeline (socket,"Erro! Nenhuma palavra ou categoria inseridas! \n");
		pthread_mutex_unlock(&lock);
	  	return;
 	}


PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ users[socket] +"'  AND admin= TRUE");
	if(PQntuples(res) == 0){
		writeline(socket, "Desculpa esta função é apenas para administradores.");
    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}
PGresult* res1 = doquery("SELECT word FROM words WHERE word='"+word+"'");
	if(PQntuples(res1) == 0){
		PGresult* res2 = doquery("SELECT cat FROM category WHERE cat='"+category+"'");
		if(PQntuples(res2) == 0){
			writeline(socket, "Desculpa esta categoria não existe. Adiciona primeiro a categoria usando \\category ");
			pthread_mutex_unlock(&lock);
			return;
		}
		PGresult* res3 = doquery("SELECT id_cat FROM category WHERE cat='"+category+"'");
		char *id;
		id=PQgetvalue(res3,0,0);
		string str(id);
		doquery("INSERT INTO words VALUES( DEFAULT,'"+str+"','"+word+"');");
		writeline (socket, "");
		writeline (socket, "Palavra adicionada com sucesso!");
		pthread_mutex_unlock(&lock);
		return;
	}
	else{
		writeline (socket, "");
		writeline (socket, "Esta palavra ja existe!");
		pthread_mutex_unlock(&lock);
		return;
	}
}
void info(int socket, string line){
pthread_mutex_lock(&lock);

string command;
string username;
string aux=line;

istringstream stream(line);

stream >> command >>username;

	if ((username == "")){
		writeline (socket,"Erro! Nenhum utilizador selecionado! \n");
		pthread_mutex_unlock(&lock);
	  	return;
 	 }
	PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ username +"'");
	if(PQntuples(res) == 0){
		writeline(socket, "Utilizador selecionado não existe \n");
    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}

	PGresult* res1 = doquery("SELECT COUNT(*) FROM game JOIN users USING(id_user) WHERE username ='"+username+"' AND gstat='f' GROUP BY id_user");
	PGresult* res2 = doquery("SELECT COUNT(*) FROM game JOIN users USING(id_user) WHERE username ='"+username+"'AND score=true AND gstat = 'f' GROUP BY id_user");
	if(PQntuples(res1)==0){
		writeline(socket, "O utilizador "+username+" ainda nao fez jogos \n");
    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}
	char *p, *w;
	p=PQgetvalue(res1,0,0);
	string strp(p);
	if(PQntuples(res2)==0){
		writeline(socket, "O utilizador "+username+", tem um score de P\\W:"+strp+"\\0 \n");
    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
  }
	w=PQgetvalue(res2,0,0);
	string strw(w);

	writeline(socket, "O utilizador "+username+", tem um score de P\\W:"+strp+"\\"+strw+" \n");
	writeline(socket, "");
	pthread_mutex_unlock(&lock);
	return;

}

void ranking(int socket){ //e mais um top 10

pthread_mutex_lock(&lock);
char *u, *w;
int aux;

PGresult* res = doquery("SELECT username , COUNT(score) FROM game JOIN users USING (id_user) WHERE score=true AND gstat='f' GROUP BY username ORDER BY COUNT(score) DESC,username ASC LIMIT 10");

for (int row = 0; row < PQntuples(res); row++) {
	u=PQgetvalue(res,row,0);
	w=PQgetvalue(res,row,1);
	string stru(u);
	string strw(w);
	aux=row+1;
	string stra=intToString(aux);
	writeline(socket,string(" ")+stra+"  username: "+stru+" vitorias: "+strw+"");
}
pthread_mutex_unlock(&lock);
return;
}

void listcategories(int socket){ 

pthread_mutex_lock(&lock);
char *c;
int a=0;

PGresult* res = doquery("SELECT cat FROM category ORDER BY  cat ASC");

if(PQntuples(res) == 0){

	writeline(socket,"Ainda não foram criadas categorias \n");
	pthread_mutex_unlock(&lock);
	return;
}

for (int row = 0; row < PQntuples(res); row++) {
	c=PQgetvalue(res,row,0);
	string strc(c);
	a=row+1;
	string strnum=intToString(a);
	writeline(socket,""+strnum+": "+strc+" ");
}
writeline(socket,"");
pthread_mutex_unlock(&lock);
return;
}

void shutdown(int socket){

	pthread_mutex_lock(&lock);


	PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ users[socket] +"'  AND admin= TRUE");
	if(PQntuples(res) == 0){
		writeline(socket, "Desculpa esta função é apenas para administradores.\n");
    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}
	else
		exit(0);

	pthread_mutex_unlock(&lock);
	return;
}
void myexit(int socket) {

    PGresult* res = doquery("SELECT username FROM users JOIN game USING (id_user) WHERE username = '"+ users[socket] +"'  AND (gstat='q' or gstat='s') ");
    if(PQntuples(res) != 0){
  		writeline(socket, "Não podes sair até acabares o jogo\n");
  	  	writeline(socket, "");
  		pthread_mutex_unlock(&lock);
  		return;
    }
    pthread_mutex_lock(&lock);
	cout << "Client " << socket << " has disconnect!\n";
    clients.erase(socket);

	close(socket);
	pthread_mutex_unlock(&lock);
	return;

 }


 void help(int socket){

 	pthread_mutex_lock(&lock);
	//alguma cena para limpar conslola

	writeline(socket,"");
	writeline(socket,"Lista de comandos :");
	writeline(socket,"");
	writeline(socket,"\\register <username> <password>");
    writeline(socket,"\\login <username> <password>");
	writeline(socket,"\\logout ");
	writeline(socket,"\\info <username>");
	writeline(socket,"\\ranking");
	writeline(socket,"\\listcat");
	writeline(socket,"\\join <id_game>");
	writeline(socket,"\\exit   (Para desconectar do servidor)");
	writeline(socket,"\\category <category>                   (Admin)");
    writeline(socket,"\\add <word> <category>               (Admin)");
    writeline(socket,"\\create <max players> <category>     (Admin) 		(category não é obrigatoria)");
	writeline(socket,"\\shutdown                              (Admin)");
  	writeline(socket,"\\start <ig_game>                       (Admin)");
	writeline(socket,"");

	pthread_mutex_unlock(&lock);

	return;
 }
 void logout(int socket)
{
    pthread_mutex_lock(&lock);

    if (online(users[socket])==false)
    {
        writeline(socket, "Nenhum utilizador online neste terminal");
		writeline(socket, "");
        pthread_mutex_unlock(&lock);
        return;
    }

    PGresult* res = doquery("SELECT username FROM users JOIN game USING (id_user) WHERE username = '"+ users[socket] +"'  AND(gstat='q' or gstat='s')  ");
    if(PQntuples(res) != 0){
  		writeline(socket, "Não podes sair até acabares o jogo");
  	  writeline(socket, "");
  		pthread_mutex_unlock(&lock);
  		return;
    }
        string username = users[socket];
        sockets.erase(username);
        users.erase(socket);
        writeline(socket,"Logout com sucesso!");
		writeline(socket, "");
        pthread_mutex_unlock(&lock);
        return;

}

void create(int socket, string line){

pthread_mutex_lock(&lock);

	string command;
	string max;
	string category;

	char *w,*c, *g, *cat;

	istringstream stream(line);
	stream >> command >> max >>category;

	PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ users[socket] +"'  AND admin= TRUE");
	if(PQntuples(res) == 0){
		writeline(socket, "Desculpa esta função é apenas para administradores.");
    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}

	if (max == ""){
		writeline (socket,"Erro, o jogo não foi criado! Não foi introduzido nenhum máximo\n");
		pthread_mutex_unlock(&lock);
	    return;
	}
	res = doquery("SELECT gstat FROM game WHERE gstat='q' or gstat='s'  ");
    if(PQntuples(res) != 0){
  		writeline(socket, "Não podes criar um jogo agora. Ainda há jogos a decorrer");
  	  writeline(socket, "");
  		pthread_mutex_unlock(&lock);
  		return;
    }

    if(category == ""){
    	res = doquery("SELECT id_word FROM words ORDER BY random() LIMIT 1");
    	w=PQgetvalue(res,0,0);
    	string strw(w);
    	PGresult* res1=doquery("SELECT id_cat,cat FROM words JOIN category USING(id_cat) WHERE id_word='"+strw+"'");
    	c=PQgetvalue(res1,0,0);
      	cat=PQgetvalue(res1,0,1);
    	string strc(c);
      	string strct(cat);
    	doquery("INSERT INTO game VALUES(DEFAULT,NULL,'"+strw+"',FALSE,'"+strc+"','"+max+"',DEFAULT,'q')");
    	PGresult* res2=doquery("SELECT id_g FROM game WHERE id_word='"+strw+"' AND id_cat='"+strc+"' ORDER BY id_g DESC");
    	g=PQgetvalue(res2,0,0);
    	string strg(g);

      	int m=stringToInt(max);

      	for(int x=0;x<(m-1);x++){

        	doquery("INSERT INTO game VALUES('"+strg+"',NULL,'"+strw+"',FALSE,'"+strc+"','"+max+"',DEFAULT,'q')");

      	}

    	writeline(socket,"Criado jogo para "+max+" jogadores com id="+strg+"\n");
    	broadcast(socket,"Criado jogo para "+max+" jogadores com id="+strg+" com a categoria: "+strct+"\n");
    	pthread_mutex_unlock(&lock);
    	return;
   	}
    PGresult* res2 = doquery("SELECT cat FROM category WHERE cat='"+category+"'");
  		if(PQntuples(res2) == 0){
	  		writeline(socket, "Esta categoria não existe. Adiciona primeiro a categoria usando \\category \n ");
	  		pthread_mutex_unlock(&lock);
	  		return;
  		}
    res2 = doquery("SELECT word FROM category JOIN words USING(id_cat) WHERE cat='"+category+"'");
      if(PQntuples(res2) == 0){
	  		writeline(socket, "Esta categoria não tem nenhuma palavra. Adiciona primeiro palavras usando \\add ");
	  		pthread_mutex_unlock(&lock);
	  		return;
  		}

   else{

   		PGresult* res3=doquery("SELECT id_cat FROM category WHERE cat='"+category+"'");
    	c=PQgetvalue(res3,0,0);
    	string strca(c);
    	PGresult* res4=doquery("SELECT id_word FROM words JOIN category USING(id_cat) WHERE cat='"+category+"'");
    	w=PQgetvalue(res4,0,0);
    	string strwo(w);
 		doquery("INSERT INTO game VALUES(DEFAULT,NULL,'"+strwo+"',FALSE,'"+strca+"' ,'"+max+"' ,DEFAULT,'q')");
    	PGresult* res5=doquery("SELECT id_g FROM game WHERE id_word='"+strwo+"' AND id_cat='"+strca+"' ORDER BY id_g DESC");
    	g=PQgetvalue(res5,0,0);
    	string strga(g);

      	int m=stringToInt(max);

      	for(int x=0;x<(m-1);x++){

        	doquery("INSERT INTO game VALUES( '"+strga+"',NULL,'"+strwo+"',FALSE,'"+strca+"','"+max+"',DEFAULT,'q')");

      	}

    	writeline(socket,"Criado jogo para "+max+" jogadores com id="+strga+" com a categoria: "+category+"\n");
    	broadcast(socket,"Criado jogo para "+max+" jogadores com id="+strga+" com a categoria: "+category+"\n");
    	pthread_mutex_unlock(&lock);
    	return;

   }
}
void join (int socket,string line){

pthread_mutex_lock(&lock);
string command;
string game;
char *m;

istringstream stream(line);
stream >> command >> game;

PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ users[socket] +"'");

	if(PQntuples(res)==0){

    	writeline (socket,"Erro! É necessario fazer login \n");
    	pthread_mutex_unlock(&lock);
      	return;
  	}

  	if (game== ""){
		writeline (socket,"Erro! Não foi introduzido nenhum jogo \n");
		pthread_mutex_unlock(&lock);
		return;
 	}

  PGresult* res1 = doquery("SELECT id_g FROM game WHERE id_g='"+game+"'");
	if(PQntuples(res1) == 0){
		writeline(socket, "O jogo selecionado não existe !");
	    writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}
  PGresult* res6 = doquery("SELECT id_g FROM game JOIN users USING (id_user) WHERE id_g='"+game+"' AND username='"+ users[socket] +"' ");

  if(PQntuples(res6)!=0){

    writeline(socket, "Ja estas neste jogo!");
    writeline(socket, "");
    pthread_mutex_unlock(&lock);
    return;
  }

  PGresult* res2 = doquery("SELECT max FROM game WHERE id_g='"+ game+"'");
  m=PQgetvalue(res2,0,0);
  string maxi(m);
  int max=stringToInt(maxi);
  res2 = doquery("SELECT max FROM game WHERE id_g='"+ game+"' AND id_user IS NOT NULL");
  if(PQntuples(res2)==max){

    writeline(socket, "O jogo selecionado já está cheio!");
    writeline(socket, "");
    pthread_mutex_unlock(&lock);
    return;
  }
  else {
	    PGresult* res4 = doquery("SELECT id_user FROM users WHERE username='"+users[socket]+"'");
	    char *i,*g;
	    i=PQgetvalue(res4,0,0);
	    res4 = doquery("SELECT id_gu FROM game WHERE id_g='"+game+"' AND id_user IS NULL");
	    g=PQgetvalue(res4,0,0);
	    string strid = string(i);
	    string strg = string(g);
	    doquery("UPDATE game SET id_user='"+strid+"' WHERE id_gu='"+strg+"'");

	    player.push_back(socket);

	    writeline(socket, "Foste adicionado ao jogo, aguarda que comece. Boa sorte! ");
	    writeline(socket, "");
	    pthread_mutex_unlock(&lock);
	    return;
  	}
}

void  startg (int socket,string line) {
  pthread_mutex_lock(&lock);
  string command;
  string game;

  istringstream stream(line);
  stream >> command >> game;

  PGresult* res = doquery("SELECT username FROM users WHERE username = '"+ users[socket] +"' AND admin= TRUE");
	if(PQntuples(res) == 0){
		writeline(socket, "Desculpa esta função é apenas para administradores.");
	    	writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
	}
  res = doquery("SELECT id_g FROM game WHERE id_g='"+game+"'");
  if(PQntuples(res) == 0){
    writeline(socket, "Desculpa o jogo selecionado não existe. \n");
		pthread_mutex_unlock(&lock);
		return;

  	}
  res = doquery("SELECT gstat FROM game WHERE id_g='"+game+"'");
  char* s;
  s=PQgetvalue(res,0,0);
  string stat(s);
  if(stat !="q"){
    writeline(socket, "O jogo selecionado já começou \n");
	  writeline(socket, "");
		pthread_mutex_unlock(&lock);
		return;
  	}
  	PGresult* res2 = doquery("SELECT max FROM game WHERE id_g='"+ game+"'");
  	char *m;
  m=PQgetvalue(res2,0,0);
  string maxi(m);
  int max=stringToInt(maxi);
  res2 = doquery("SELECT max FROM game WHERE id_g='"+ game+"' AND id_user IS NOT NULL");
  if(PQntuples(res2)!=max){

    writeline(socket, "O jogo selecionado ainda não está cheio!");
    writeline(socket, "");
    pthread_mutex_unlock(&lock);
    return;
  }

    doquery("UPDATE game SET gstat='s' WHERE id_g='"+game+"'");
    turn=player[0];

    res = doquery("SELECT word FROM game JOIN words USING(id_word) WHERE id_g="+game+"");
    char *aux=PQgetvalue(res,0,0);
    string straux(aux);
    w=straux;
    cop=straux;

    for(int l=0; l!=straux.length(); l++){
      wstat += "_";
    }




  writeline(socket,"Jogo "+game+" vai começar\n ");
  broadcast(socket,"Jogo "+game+" vai começar \n");
  broadcast(socket,""+wstat+"");
  broadcast(socket,"O jogador "+users[turn]+" é o primeiro a jogar!\n");

  pthread_mutex_unlock(&lock);
  return;
}

void guess (int socket, string line){
  pthread_mutex_lock(&lock);
  string command;
  string guess;

  istringstream stream(line);
  stream >> command >> guess;

  if(socket!=turn){

    writeline(socket,"Não é a tua vez!\n");
    writeline(socket,"");
    pthread_mutex_unlock(&lock);
    return;
  }
  if(guess==""){

    writeline(socket,"Não inseriste nenhuma letra \n");
    pthread_mutex_unlock(&lock);
    return;
  }
  if(guess.length() > 1){
    if(guess==w){


        PGresult* res = doquery("SELECT id_g, id_gu FROM game JOIN users USING (id_user) JOIN words USING (id_word) WHERE username = '"+ users[socket] +"' and word='"+w+"'");
        char* ga=PQgetvalue(res,0,0);
        string game(ga);
        res = doquery("SELECT id_gu FROM game JOIN users USING (id_user) JOIN words USING (id_word) WHERE username = '"+ users[socket] +"' and word='"+w+"'");
        char* g=PQgetvalue(res,0,0);
        string gu(g);

        doquery("UPDATE game SET gstat='f' WHERE id_g='"+game+"'");
        doquery("UPDATE game SET score=TRUE WHERE id_gu='"+gu+"'");

        writeline(socket,"Parabens venceste o jogo!\n \n");
        broadcast(socket,"O user "+ users[socket] +" ganhou, a palavra era "+guess+"!\n \n");
      	player.clear();
      	wstat.clear();
      	cop.clear();
      	w.clear();
      	list.clear();
        erros=0;
        pthread_mutex_unlock(&lock);
        return;
    }
    else{

        if (erros == (3*player.size()))
        {
        	writeline(socket,"Jogo Terminado! Atingiram o numero maximo de tentativas!\n");
        	broadcast(socket,"Jogo Terminado! Atingiram o numero maximo de tentativas!\n");

          PGresult* res = doquery("SELECT id_g FROM game JOIN users USING (id_user) JOIN words USING (id_word) WHERE username = '"+ users[socket] +"' AND word='"+w+"' AND gstat='s'");
          char* ga=PQgetvalue(res,0,0);
          string game(ga);

          doquery("UPDATE game SET gstat='f' WHERE id_g='"+game+"'");

        	player.clear();
			    wstat.clear();
			    cop.clear();
			    w.clear();
			    list.clear();
			    erros = 0;

        	pthread_mutex_unlock(&lock);
        	return;
        }
        erros++;
        writeline(socket,"Tentativa errada! "+intToString(erros)+" de "+intToString(3*player.size())+" tentativas usadas \n");
        broadcast(socket,"O user "+ users[socket] +" tentou "+guess+" e errou! "+intToString(erros)+" de "+intToString(3*player.size())+" tentativas usadas \n");
        turns();
        writeline(turn,""+ users[turn] +" é agora a tua vez \n");

      pthread_mutex_unlock(&lock);
      return;
    }
  }
  else{
	for(int n=0;n<list.size();n++){

		if(list[n]==guess){
		writeline(socket,"Esta letra já foi tentada, tenta outra vez! \n");
      		pthread_mutex_unlock(&lock);
      		return;
		}
	}

    if(cop.find(guess) != -1){//caso em que existe
      while(cop.find(guess) != -1){
      wstat.replace(cop.find(guess), 1, guess);
      cop.replace(cop.find(guess), 1, "_");
      }
	list.push_back(guess);
      broadcast(socket,""+wstat+"\n");
	writeline(socket,""+wstat+"\n");
      if (w==wstat) {
        broadcast(socket,"Jogo terminado! A palavra era "+wstat+"");
        PGresult* res9 = doquery("SELECT id_g FROM game JOIN users USING (id_user) JOIN words USING (id_word) WHERE username = '"+ users[socket] +"' and word='"+w+"' and gstat='s'");
        char* gam=PQgetvalue(res9,0,0);
        string games(gam);
        doquery("UPDATE game SET gstat='f' WHERE id_g='"+games+"'");
	res9 = doquery("SELECT id_gu FROM game JOIN users USING (id_user) JOIN words USING (id_word) WHERE username = '"+ users[socket] +"' and id_g='"+games+"'");
        char* g=PQgetvalue(res9,0,0);
        string gu(g);

	doquery("UPDATE game SET score=TRUE WHERE id_gu='"+gu+"'");
	writeline(turn,"Parabéns "+ users[turn] +", a palavra era "+wstat+"\n\n");
	player.clear();
	wstat.clear();
	cop.clear();
	w.clear();
	list.clear();
  erros=0;

	pthread_mutex_unlock(&lock);
      	return;

      }
      writeline(turn,""+ users[turn] +" é agora a tua vez\n");
      pthread_mutex_unlock(&lock);
      return;

    }
    else{   //caso em que n existe
      if (erros == (3*player.size()))
      {
        writeline(socket,"Jogo Terminado! Atingiram o numero maximo de tentativas!\n");
        broadcast(socket,"Jogo Terminado! Atingiram o numero maximo de tentativas!\n");

        PGresult* res = doquery("SELECT id_g FROM game JOIN users USING (id_user) JOIN words USING (id_word) WHERE username = '"+ users[socket] +"' AND word='"+w+"' AND gstat='s'");
        char* ga=PQgetvalue(res,0,0);
        string game(ga);

        doquery("UPDATE game SET gstat='f' WHERE id_g='"+game+"'");

        player.clear();
        wstat.clear();
        cop.clear();
        w.clear();
        list.clear();
        erros = 0;

        pthread_mutex_unlock(&lock);
        return;
      }
      erros++;
      writeline(socket,"Esta letra nao existe, perdeste a vez! "+intToString(erros)+" de "+intToString(3*player.size())+"  tentativas usadas \n");
      broadcast(socket,""+wstat+"");
      turns();
	list.push_back(guess);
      writeline(turn,""+ users[turn] +" é agora a tua vez! "+intToString(erros)+" de "+intToString(3*player.size())+"  tentativas usadas \n");
      pthread_mutex_unlock(&lock);
      return;
    }
  }


}

// handles one client
void* client (void* args) {
  int sockfd = *(int*)args;
  string line;

  clients.insert(sockfd);

  cout << "Reading from socket " << sockfd << endl;
while (readline(sockfd, line)) {
   if(line.find("\\register")==0){
   	registuser(sockfd,line);}
else if((line.find("\\login")==0)){
	 login(sockfd,line);}
else if((line.find("\\shutdown")==0)){
	 shutdown(sockfd);}
else if((line.find("\\category")==0)){
	 cat(sockfd,line);}
else if((line.find("\\add")==0)){
	 add(sockfd,line);}
else if((line.find("\\info")==0)){
	 info(sockfd,line);}
else if((line.find("\\logout")==0)){
	 logout(sockfd);}
else if((line.find("\\exit")==0)){
	 myexit(sockfd);}
else if((line.find("\\help")==0)){
	 help(sockfd);}
else if((line.find("\\ranking")==0)){
	 ranking(sockfd);}
else if((line.find("\\create")==0)){
	 create(sockfd,line);}
else if((line.find("\\join")==0)){
 	 join(sockfd,line);}
else if((line.find("\\start")==0)){
   	 startg(sockfd,line);}
else if((line.find("\\guess")==0)){
     guess(sockfd,line);}
else if((line.find("\\listcat")==0)){
     listcategories(sockfd);}

}

  clients.erase(sockfd);

  close(sockfd);
}
int main (int argc, char *argv[])
{

dbconn();

pthread_mutex_init(&lock,NULL);

  /* Data structures */
  int sockfd, newsockfd, port = 6000;
  socklen_t client_addr_length;
  struct sockaddr_in serv_addr, cli_addr;
  string line;

  /* Initialize the socket
     AF_INET - We are using IP
     SOCK_STREAM - We are using TCP
     sockfd - id of the main server socket
     If it returns < 0 an error occurred */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
     cout << "Error creating socket" << endl;
     exit(-1);
  }

  /* Create the data structure that stores the server address
     bzero - cleans the structure
     AF_INET - IP address
     INADDR_ANY - Accept request from any IP address */
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  /* Bind the socket. The socket is now active but we
     are still not receiving any connections
     If it returns < 0 an error occurred */
  int res = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (res < 0) {
     cout << "Error binding to socket" << endl;
     exit(-1);
  }

  /* Start listening to connections. We want to have
     at most 5 pending connections until we accept them */
  listen(sockfd, 5);

  while (true) {
    /* Accept a new connection. The client address is stored as:
       cli_addr - client address
       newsockfd - socket id for this client */
    client_addr_length = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_length);

    /* Create a thread to handle this client */
    pthread_t thread;
    pthread_create(&thread, NULL, client, &newsockfd);
}
		dbconnend();
  	close(sockfd);
  	pthread_mutex_destroy(&lock);
  	return 0;
}
