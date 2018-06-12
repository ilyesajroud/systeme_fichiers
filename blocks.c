#include "blocks.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
int block_number=-1;
int f=-1; //fichier 
void open_disk ()
{
	struct stat st;
	
	
	f = open ("./vdisk", O_RDWR);
	fstat (f, &st);
	block_number = st.st_size / 1024;
	printf(" ouverture du disque, nombre de block  : %d\n",block_number);
	

}

int read_block(int number, void * block ) // plusieur type de blocks (super block , inode block données ...)
{
	if (number < 0 &&  number >= block_number)
	{
		printf ("debordement du nombre de block\n");
		return -1;
	}
	lseek(f,number * BLOCKSIZE ,SEEK_SET); // lseek pointe sur un point dans un fichier 
	// SEEK_SET ensemble de recherche dans le fichier 
	read(f,block,BLOCKSIZE); // lecture 
	return 0;	
}
int write_block(int number, void * block)
{
	if (number < 0 &&  number >= block_number)
	{
		printf ("debordement du nombre de block\n");
		return -1;
	}
	lseek(f,number * BLOCKSIZE ,SEEK_SET);
	write(f,block,BLOCKSIZE); // ecriture 
	fsync(f); // synchonisation du fihchier ( disque dur)
	return 0;
}
int write_inode(int number , void * inode)
{
	if (number == 0 || number >= block_number  )
	{
		printf ("debordement du nombre d'inode ecriture \n");
		return -1;
	}
	if (lseek(f, INODE_TABLE_START * BLOCKSIZE + number * 256 ,SEEK_SET) < 0) // taille inode 256 bytes (bytes en ENG)
	{
		perror("positionement dans le disque echoué\n");
		return -1;
	}
	if (write(f,inode,256)!=256)
	{
		perror("ecriture echoué\n");
		return -1;
	}
	
	if (fsync(f) < 0)
	{
		perror("fsync");
	}
	return 0;
}
int read_inode(int number , void * inode)
{
	if (number == 0 || number >= MAX_FILE_NUM )
	{
		printf ("debordement du nombre d'inode lecture \n");
		return -1;
	}
	lseek(f,INODE_TABLE_START * BLOCKSIZE + number * 256,SEEK_SET);
	read(f,inode,256);
	return 0;
}
void format_disk ()
{
	open_disk(); // ouverture du disque dur virtuel
	struct super_block *sb = (struct super_block *) calloc(BLOCKSIZE, 1); // allocation du superblock
	char* test = (char*) calloc (BLOCKSIZE,1); // bloc a lire
	struct inode *root = (struct inode *) malloc(sizeof(struct inode)); // allocation inode root
	struct dir *root_block = (struct dir *) malloc(sizeof(struct dir)); // allocation bloque de root repertoire
	struct ind_block  ind_init;
	int i; // compteur 
	read_block(0,test);
	if (* (int *) test == 2012) // test si le disque est deja formaté 
	{
		printf("disque dur deja formaté \n");
	}
	else
	{	
		sb->magic_number = 2012;       //ecrire le nombre magique 
		// ce nombre magique caracterise notre disque dur
		sb->block_num = block_number; // contient le nombre de block total dans le disque
		sb->inode_num = MAX_FILE_NUM;
		sb->free_blk = 99883;    // total is 99884, minus 1 for root dir block
		sb->free_inode = 2499;   // total is 2500, minus 1 for root dir inode
		write_block(0, sb); // ecriture du superblock
		// superblock ecrit [X]
		for (i = 0 ; i < MAX_OPEN_FILES ; i++)
		{
			fd[i] = -1;
		}
		// initialisation des descripteurs
		*test = 0x80 ; // @ de depart des blocks 
		write_block(BLOCK_MAP_START,test);
		*test = 0xc0; // saut du premier inode puis @ de depart des inodes
		write_block(INODE_MAP_START,test);
		// carte des inodes et bloque initialisé 
		root->type = DIR_TYPE; // type dossier
		root->num = 1;                // le deuxieme inode est l'inode root,a mettre dans le premier bloque de données (bloque 16 dans le disque,le nombre d'inode 1
		root->size = 1;               // 1 KB  pour bloque
		root->uid = 0; // id user 
		root->gid = 0; // id groupe
		strcpy(root->mode, "rwxr-xr-x"); // droit du repertoire root
		strcpy(root->name, "/"); // nom du repertoire root 
		root->blocks[0] = BLOCK_TABLE_START;     // ou est stocké le repertoire root dans le disque 
		for(i = 1; i < 10; i++)
			root->blocks[i] = -1;    // les autres blocs sont a null on pas depassé qté de données d'un bloque de données
		for (i=0;i<256;i++)
		{
			ind_init.blocks[i]=-1;
		}

		for(i = 0; i < 30; i++)
			root->ind_blocks[i] = ind_init; // meme pour les blocs indirects puisque les autres bloques son vides
		write_inode(1, root);
		printf("inode root ecrit \n");
		// inode root ecrit [X]
		// dentry soit disant la colle entre les repertoires et fichier du disque
		// definition vers quel inodes pointes les chemins
		root_block->dentry[0].inode = 1; // inode du root 
		root_block->dentry[0].type = DIR_TYPE; // type deja definit
		root_block->dentry[0].length = 32; // taille 
		strcpy(root_block->dentry[0].name, "."); // . est le chemin courant du fichier ( lui meme)

		root_block->dentry[1].inode = 1;
		root_block->dentry[1].type = DIR_TYPE;
		root_block->dentry[1].length = 32;
		strcpy(root_block->dentry[1].name, "..");  // .. chemin des parent dans le cas du root (lui meme)
		write_block(BLOCK_TABLE_START, root_block); // ecriture du bloque de root 
		printf("bloque données root ecrit \n");
		free(test);
		free(sb);
		free(root);
		free(root_block);
		printf("disque dur formaté ,systeme de fichier initialisé !\n");

	}
}
struct inode* find(const char* path)
//trouver un inode a partir du chemin du fichier / repertoire donné 
// distinction entre repertoire et fichie suivant le dernier caractere du chemin "/"
{
	struct inode* root = (struct inode *) malloc(sizeof(struct inode));
	//stocker le root dans cet inode si il s'agit de l'inode du root dans le chemin
	struct inode* aux = root;
	//dans aux l'inode recherché sera stocké 
	struct dir* direc = (struct dir *) malloc(sizeof(struct dir));
	//on stockera le dentry suivant les repertoire dans le chemin
	int count = 0;
	//le nombre de repertoires dans le chemins
	char arg[MAX_LEVEL][MAX_FILE_NAME_LENGTH]={{0},{0}};
	//stocker les noms de repertoire suivant les etages
	//notre programme supporte que 10 de repertoire il faut pas deppasser le MAX_LEVEL
	//MAX_FILE_NAME_LENGTH : comme son nom l'indique est la taille max d'un nom de repertoire
	int i=0;
	int j=0;
	int k=0;
	//compteurs
	int abs = 0 ;
	//marqueur de chemin absolu
	int d = 0 ;
	//marqueur de repertoire ou fichier suivant le chemin
	// cas de chemin est celui du root 
	if (!strcmp(path,"/"))
	{
		read_inode (1,root);
		//1 numero du root
		return root;
		//inode trouvé 
	}

	if (path[strlen(path)-1]== '/')
	{
		d=1; // il s'agit d'un repertoire
	}
	if (path[0] == '/')
	{
		abs=1; // il s'agit d'un chemin absolu 
		i=1; // lire la chaine du chemin a partir de 1 pour ignorer le '/'
		//lors du stockage dans arg
	}
	while (path[i]!= '\0')
	{
		if ( j == MAX_LEVEL)
		{
			printf("Erreur chemin\n");
			return NULL;
			// nous avons depassé ici les etage supporté de sous repertoire 
			//=> erreur chemin
		}

		if (path[i] != '/')
		//si on a pas atteint la fin du nom de repertoire courant
		{
			arg[j][k]=path[i];
			i++;
			k++;
			//mettre les caractere du nom dans l'etage approprié
		}
		else
		{
			arg[j][k-1]='\0';
			//conclure la chaine
			i++;
			j++;
			// j++: commencer a lire le sous dossier suivant, augmenter l'etage
			k=0; // stocker le nom depuis l'indice 0 certainement
		}

	}
	if (d == 1)
	//le cas de repertoire
	count= j-1; // j (nombre d'etage trouvé) -1 : j sera incrementé (existance de '/' a la fin de la chaine)
	else
	count= j+1;
	//nom fichier est dans la chaine suivant le '/'

	
	if  (abs == 1)
	{
		read_inode(1,aux);
		//commencer depuis le root
	}
	else
	{
		read_inode(cwd_inode,aux);
		//cwd_inode designe le repertoire courant
		//commencer depuis ce repertoire
	}
	for (i=0;i<count;i++)
	//parcourir les etages trouvé et chercher l'inode
	{	

		
		int tst =0; //tester si on a trouvé l'inode cherché
		for (j=0;j<10;j++)
		//parcourir les blocks
		{
			if ( aux->blocks[j] <= 0 )
			{
				continue;
				//aller a l'iteration suivante si le bloc est non utilisé
				//initialisé a 0 ou -1
			}
			//lecture de la dentry du bloc aux->blocks[j] est le numero du bloc
			read_block(aux->blocks[j],direc);
			for(k=0;k<32;k++)
			//parcour du dentry lu
			{
				
				if (direc->dentry[k].inode==0)
				{
					continue;
					//aller a l'iteration suivante si l'inode du dentry n'est pas utilisé
					//(initialisé a 0)
				}
				//sinon on compare les types
				if (d==1 && direc->dentry[k].type==FILE_TYPE)
				{
					continue;
					//aller a l'iteration suivante 
					//nous cherchons un repertoire et le dentry selectionné conserne un fichier
				}
				if (d==0 && direc->dentry[k].type==DIR_TYPE)
				{
					continue;
					//aller a l'iteration suivante 
					//nous cherchons un fichier et le dentry selectionné conserne un repertoire
				}
				//sinon les deux type sont identiques
				//=> comparaison de noms
				if (!strcmp(direc->dentry[k].name,arg[i]))
				// égalité => inode trouvé
				{
					read_inode(direc->dentry[k].inode,aux);
					//stocker dans aux l'inode dans le numero "direc->dentry[k].inode"
					//ou se trouve l'inode recherché 
					tst=1; 
					break ; // on s'arrete (on a trouvé et recuperé l'inode)
				}
				if (direc->dentry[k].length == 64 )
				{
					k++; 
					//le contenu dans dentry est stocké dans plus qu'un dentry (nom)
					// longeur max 64 depassé
					//on augmente le k alors pour passer la dentry suivante
				}	
			}
			if (k<32)
			{
				break;
				// on a trouvé l'inode (break dans la boucle precedente)
			}
		}
		if (!tst)
		{
			printf("le chemin %s n'est pas trouvé \n", path);
			return NULL;
			//si on a parcouru toutes les etages et le fichier est non trouvé (tst est inchagé =0)
			//renvoyer null
		}
	}
	return aux;
	
}
int get_block()
//renvoyer le numero d'un bloque vide
{
	int found = 0;
	//marque si bloque est trouvé ou pas
	int i,j,k;
	//compteurs
	int retval;
	//le numero du bloque a recuperer
	unsigned char bitb;
	char bits[BLOCKSIZE];
	//variable ou stocker le block trouvé

	for (i=BLOCK_MAP_START ; i < INODE_MAP_START ; i++)
	//chercher dans la carte des bloques
	//trouver le numero du bloque
	{
		read_block(i,bits);
		for (j=0; j< BLOCKSIZE ;j++)
		//parcour du bloque lu
		//chercher le nombre de bit 
		//s'arreter au premier bit null 
		{
			if (bits[j] != '\xff')
			{
				found =1;
				//byte dans le bloque trouvé 
				break;
				//arreter la recherche
			}
		}
		if (found)
		{
			break;
			//s'arreter si on a trouvé le bloque
		}	
	}
	bitb = (unsigned char ) bits[j];
	//recuperer le byte trouvé 
	for (k=0;k<8;k++)
	{
		if((~(bitb << k) & 0x80) != 0)
		{
			break;
			//chercher le premier indice de bit 1 
		}
	}
	retval = k + j * 8 + (i-BLOCK_MAP_START) * 8192 + BLOCK_MAP_START;
	// k indice du bit trouvé j l'indice du byte contenant le bit
	//i l'indice du bloque dans la carte de bloque 8192 est le nombre de bloques dans le disque
	//BLOCK_TABLE_START ou commence les bloque => recuperer l'indice du bloque dans le disque
	if (retval >= 102400) //nombre de bloques possibles
	{
		printf("erreur : pas de bloques libres \n");
		return -1;
	}
	bits[retval >> 3] |= (1 << (8 - 1 - (retval & 0x07)));
	//decaler retval de 3 bits , reglages du bitmap des blocks
	write_block(i,bits);
	//ecriture du bloque
	return retval;
	//renvoi du nombre de bloque libre créé
}
int get_inode() {
//meme principe que get_block()
//on cherche ici un inode 
    int found = 0;
    int i, j, k;
    int ret;
    unsigned char bitb;
    char bits[BLOCKSIZE];
    for(i = INODE_MAP_START; i < INODE_TABLE_START; i++)
    //changement de l'interval de recherche 
    //on cherche dans la carte des inodes	
    {
        read_block(i, bits);
        for(j = 0; j < BLOCKSIZE; j ++){
            if(bits[j] != '\xff') {
                found = 1;
                break;
            }
        }
        if(found) break;
    }
    bitb = (unsigned char)bits[j];
    for(k = 0; k < 8; k ++)
    {
       	if((~(bitb << k) & 0x80) != 0)
       	{
       		break;
       	}
    } 

    ret = k + j * 8 + (i - INODE_MAP_START) * 8192 + INODE_TABLE_START;
    // changemennt des parametre des inodes 
    if (ret >= MAX_FILE_NUM) 
    {
        printf("erreur : pas d'inodes libres\n");
        return -1;
    }
    bits[ret >> 3] |= (1 << (8 - 1 - (ret & 0x07)));             
    write_block(i, bits);

    return ret;
}
int mycreat(const char* path)
//creer et ouvrir le fichier dans le chemin
{
	int i =0, j, k; //compteurs
	int div;
	//marqueur de division de chaine
	int newinode;
	//numero de l'inode vide puis initialiser le fichier
	int empty_dentry;
	//numero de la dentry vide puis l'initialiser
	char dir_name[MAX_FILE_NAME_LENGTH * MAX_LEVEL];
	//nom du repertoire
	char file_name[MAX_FILE_NAME_LENGTH];
	//nom du fichier 
	struct inode *cur;
	//inode du repertoire ou on va creer le fichier
	struct inode tmp_inode;
	//inode a creer et stocker dans le disque
	struct dir tmp_direc;
	//la dentry a creer et stocker
	char bits[BLOCKSIZE]; // bloc de bit map
	int found = 0; // marqueur si on trouve un espace vide dans le bitmap
	int nb=0;
	struct ind_block  ind_init;
	//separer le nom du fichier du repertoire
	while(path[i]!='\0')
	{
		if(path[i] == '/')
			div=i;
		i++;
		//trouver le dernier '/' dans la chaine
	}

	strncpy(dir_name,path,div+1);
	//stocker le nom du chemin
	dir_name[div+1]='\0';
	//cloturer la chaine dir_name
	strncpy(file_name,path+div+1,i-div-1);
	//stocker le nom du fichier
	file_name[i-div-1] ='\0';

	cur=find(dir_name);
	//recuperer l'inode du repertoire courant
	//verification de l'existance du repertoire
	if (cur == NULL)
	{
		printf("repertoire inexistant : %s\n",dir_name);
	}
	//si oui
	//verification si le repertoire est bel et bien un repertoire et non un fichier
	if (cur->type == FILE_TYPE)
	{
		printf("%s est un fichier \n",dir_name );
		//on ne peut pas creer de fichiers dans un fichier 
	}
	//verification si le fichier existe deja dans le repertoire
	for(i=0;i<10;i++)
	//parcourir les blocks
	{
		if (cur->blocks[i] == -1 )
		{
			continue ;
			//aller vers l'iteration suivante
			//bloque non utilisé
		}
		read_block(cur->blocks[i],&tmp_direc);
		//lecture du bloque
		for (j=0;j<32;j++)
		{
			if (tmp_direc.dentry[j].inode !=0 && tmp_direc.dentry[j].type==FILE_TYPE && strcmp(tmp_direc.dentry[j].name, file_name) == 0 )
			//si l'inode du dentry est utilisé et il a le type d'un fichier et le nom du fichier a créé est le meme dans le dentre 
			{
				//fichier deja existant !
				printf("Fichier deja existant \n");
				return -1 ; 
				//sortie erreur de creation du fichier 
			}
		}
	}
	//sinon recherche de dentry vide ou mettre le fichier

	for (i=0;i<10;i++)
	//parcour des bloque
	{
		if (cur->blocks[i] != -1)
		{
			read_block(cur->blocks[i],&tmp_direc);
			//lecture du bloque
			for (j=0;j<32;j++)
			//parcour des dentry
			{
				if (tmp_direc.dentry[j].inode==0)
				//inode vide trouvé
				{
					empty_dentry=j;
					//stocker le nombre du dentry vide
					break;
					//arreter la recherche
				}
			}
			if (j<32)
			{
				break ;
				//dentry vide deja trouvé 
			}
		}
		else
		{
			//block courant est deja plein 
			//creation d'un nouvel bloque
			//le mettre dans la position i 
			cur->blocks[i]= get_block();
			read_block(cur->blocks[i],&tmp_direc);
			empty_dentry = 0;
			//stocker le nombre du dentry vide
			break;

		}
	}

	// recuperation d'un inode vide 
	newinode = get_inode();
	if (newinode == -1 )
	{
		printf("espace insuffisant \n");
		return -1;
	}
	tmp_direc.dentry[empty_dentry].inode=newinode; // initialiser l'inode dans la dentry a partir du numero du nouvel inode
	tmp_direc.dentry[empty_dentry].type= FILE_TYPE;  //le type est un fichier dans la dentry
	if(strlen(file_name) <32)
	{
		tmp_direc.dentry[empty_dentry].length = 32 ;
		//longeur du nom
	}
	else
	{
		tmp_direc.dentry[empty_dentry].length = 64 ;	
	}
	//stocker le nom du fichier dans la dentry
	strncpy(tmp_direc.dentry[empty_dentry].name ,file_name, strlen(file_name));
	write_block(cur->blocks[i],&tmp_direc);
	//maj du dentry aprés insertion des infos du fichiers
	//initialisation de l'inode 
	read_inode(newinode,&tmp_inode);
	//lecture de l'inode
	tmp_inode.type= FILE_TYPE;
	//initialisation du type (fichier)
	tmp_inode.num=newinode;
	//initialisation du numero
	strcpy (tmp_inode.mode, "rw-rw-r--"); 
	//mettre les droits par defaut 
	tmp_inode.size =0 ; // en bytes vide pour le moments
	//
	//prop groupe
	//stocker le nom du fichier dans l'inode
	strncpy(tmp_inode.name,file_name,strlen(file_name));
	tmp_inode.blocks[0]=get_block();
	//bloque 0 vide
	for(i=1;i<10;i++)
	{
		tmp_inode.blocks[i]=-1;
	}// les autre bloques sont non aloués
	for (i=0;i<256;i++)
	{
		ind_init.blocks[i]=-1;
	}
	for (i=0;i<30;i++)
	{
		tmp_inode.ind_blocks[i]=ind_init;
		//les bloques indirectes sont non aloués certainement
	}
	//sauvegarde de l'inode 
	write_inode(newinode,&tmp_inode);
	//ouverture du fichier créé
	int ret = -1 ;
	for (i = 0 ;i<MAX_OPEN_FILES ;i++)
	//chercher descripteur libre 
	{
		if (fd[i]== -1 || fd[i] == 0)
		{
			//si descripteur vide ou fermé 
			fd[i]= newinode ;
			//ouvrir le ficher
			ret=i;
			break;
			//arreter la recherche descripteur libre trouvé
		}
	}
	for(i = INODE_MAP_START; i < INODE_TABLE_START; i ++)
    //changement de l'interval de recherche 
    //on cherche dans la carte des inodes	
    {
        read_block(i, bits);
        for(j = 0; j < BLOCKSIZE; j ++){
            if(bits[j] != '\xff') {
                found = 1;
                nb=i;
                break;
            }
        }
        if(found) break;
    }
    bits[j]='\xff';//changement de la carte 
    write_block(nb,bits);//mise a jour du bitmap
    for (i=BLOCK_MAP_START ; i < INODE_MAP_START ; i++)
	//chercher dans la carte des bloques
	//trouver le numero du bloque
	{
		read_block(i,bits);
		for (j=0; j< BLOCKSIZE ;j++)
		//parcour du bloque lu
		//chercher un bit vide , un bit rempli contient 0xff
		{
			if (bits[j] != '\xff')
			{
				found =1;
				//byte dans le bloque trouvé 
				nb=i;
				break;
				//arreter la recherche
			}
		}
		if (found)
		{
			break;
			//s'arreter si on a trouvé le bloque
		}	
	}
	bits[j]='\xff'; //changemet de la carte 
	write_block(nb,bits);// mise a jour du bit map

	if (ret == -1 )
	{
		printf("nombre maximal de fichier ouvert atteint, ouverture impossible\n");
	}
	printf("le fichier %s a ete créé avec succes et ouvert dans le descripteur %d\n",file_name,ret);
	free(cur); //vider le repertoire aloué

	return ret;
}
void my_close(const char* path)
{
	
	struct inode *cur = (struct inode *) malloc(sizeof(struct inode));
	int numb=-2; // conteneur du numero inode 
	int found =-1; // marquer de test
	int i; // compteur
	cur = find(path); // recherche l'inode du fichier
	if (cur ==  NULL)
	//test si le fichier est introuvable 	
	{
		printf("fichier %s inexistant\n",path);
	}
	else 
	if (cur->type == DIR_TYPE )
	// test s'il s'agit d'un repertoire
	{
		printf("%s n'est pas un fichier\n",path);
	}
	else
	{
		numb = cur->num ;
		
		for (i=0;i<MAX_OPEN_FILES;i++)
		//chercher et fermer le fichier 
		// on stocke dans la table des fichier ouvert le numero de l'inode 
		{
			if (fd[i]==numb)
			{
				fd[i]=-1;
				found=1;
				printf("%s fermé\n",path );
				break;
			}
		}
		if (found==-1)
		//si on a pas trouvé le fichier dans fd => fichier fermé 
		printf("fichier deja fermé\n");
		free (cur);
	}

}

int my_open(const char* path)
{
	int numb=-1;
	int ret=-1;
	struct inode *cur = (struct inode *) malloc(sizeof(struct inode));
	int i;
	cur = find (path);
	if (cur == NULL)
	{
		printf("fichier %s inexistant\n",path);
		return -1;
	}
	if (cur->type == DIR_TYPE )
	// test s'il s'agit d'un repertoire
	{
		printf("%s n'est pas un fichier\n",path);
	}
	numb = cur-> num ;
	for (i=0;i<MAX_OPEN_FILES;i++)
	{
		if (fd[i]==0 || fd[i]==-1)
		{
			fd[i]=numb;
			ret=i;
			printf("%s ouvert dans le descripteur %d\n",path,i);
			break;
		}
	}
	if (ret == -1)
	{
		printf("nombre maximal de fichier ouvert atteint, ouverture impossible\n");
	}

}
int my_read (int fdd, void * buf ,int nbtes)
{
	struct inode *cur = (struct inode *) malloc(sizeof(struct inode));
	unsigned char tmp[BLOCKSIZE];
	int i,j;
	int blk = (nbtes / 1024) + 1 ; // calcul du nombre de blocs a parcourir 
	// a lire depuis chacun 1024 sauf le dernier bloc a lire lstblk ( si lstblk !=0 )
	int lstblk = nbtes % 1024 ;
	int len = 0; // longeur du buf 
	int rdnb = BLOCKSIZE;
	int nbind=0; // nombre de blocs indirects a parcourir
	int lstind=0;
	if (fd[fdd]==0 || fd[fdd]== -1)
	{
		printf("lecture :le fichier n'est pas ouvert \n" );
	}
	else
	{ // lecture
		read_inode(fd[fdd],cur);
		if (blk <=10)
		//lecture seulement des blocs directs
		{
			for (i=0;i<blk;i++)
			{ // parcour des blocs
				if (i<blk-1) 
				{
					rdnb=BLOCKSIZE;
				}
				else
				{
					if (lstblk == 0)
						// si le reste de la division par 1024 (BLOCKSIZE) est > 0
						// on met ce rest rdnb (pour ne pas lire plus qu'a demandé l'utilisateur)
						rdnb = BLOCKSIZE;
					else
						rdnb = lstblk;
				}
				if (read_block(cur->blocks[i],tmp)!=-1) // lecture du bloc a copier 		
				{
					memcpy(buf + len , tmp ,rdnb); // memcpy copie du bloc dans buf 
					// on decale le pointeur de buf pour ne pas ecraser l'ancienne lecture
					len+=rdnb;
				}
				else
					printf("erreur lecture fichier\n");
			}
		}
		else 
		{ // lecture des blocs direct et indirects 

			// lecture des bloc directs avants
			for (i=0;i<10;i++)
			//il y'a que 10 blocs directs
			{
				if (read_block (cur->blocks[i],tmp)!=-1)
				{
					memcpy(buf+len,tmp,BLOCKSIZE);
					len+=BLOCKSIZE;
				}
				else
				{
					printf("erreur lecture fichier\n");
				}
			}
			nbind = ((blk -10) / 256) + 1;
			//chaque bloc indirect contient 256 blocs directs 
			// on a deja parcouru 10 blocs
			for (i=0;i<nbind-1;i++)
			{
				for (j=0;j<256;j++)
				{
					if (read_block ((cur->ind_blocks[i]).blocks[j],tmp)!=-1)
					//lire les blocs indirects
					{
						memcpy(buf+len,tmp,BLOCKSIZE);
						len+=BLOCKSIZE;
					}
					else
					{
						printf("erreur lecture fichier\n");
					}	
				}
			}
			lstind = (blk-10) % 256;
			//nombre de dernier blocs idirects a parcourir
			if (lstind == 0) 
				lstind = 256;
			for (j=0;j<lstind;j++)
			{
				if (i<lstind-1) 
				{
					rdnb=BLOCKSIZE;
				}
				else
				{
					if (lstblk == 0)
						// si le reste de la division par 1024 (BLOCKSIZE) est > 0
						// on met ce rest rdnb (pour ne pas lire plus qu'a demandé l'utilisateur)
						rdnb = BLOCKSIZE;
					else
						rdnb = lstblk;
				}
				if (read_block((cur->ind_blocks[i]).blocks[j],tmp)!=-1) // lecture du bloc a copier 		
				{
					memcpy(buf + len , tmp ,rdnb); // memcpy copie du bloc dans buf 
					// on decale le pointeur de buf pour ne pas ecraser l'ancienne lecture
					len+=rdnb;
				}
				else
					printf("erreur lecture fichier\n");
			}




		}

	}

	return len;
}

int my_write (int fdd, char * buf ,int nbtes)
{
	struct inode *cur = (struct inode *) malloc(sizeof(struct inode));
	unsigned char tmp[BLOCKSIZE];
	int i,j;
	int blk = (nbtes / 1024) + 1 ; // calcul du nombre de blocs a parcourir 
	// a ecrire depuis chacun 1024 sauf le dernier bloc, que lstblk a ecrire ( si lstblk !=0 )
	int lstblk = nbtes % 1024 ;
	int len = 0; // longeur du buf 
	int rdnb = BLOCKSIZE;
	int nbind=0; // nombre de blocs indirects a parcourir
	int lstind=0;
	if (fd[fdd]==0 ||fd[fdd]==-1)
	{
		printf("ecriture: le fichier n'est pas ouvert \n");
	}
	else
	{
		// ecriture 
		read_inode(fd[fdd],cur);
		if (blk<=10)
		{
			for (i=0;i<blk;i++)
			{ // parcour des blocs
				if (i<blk-1) 
				{
					rdnb=BLOCKSIZE;
				}
				else
				{
					if (lstblk == 0)
						rdnb = BLOCKSIZE;
					else
						rdnb = lstblk;
				}
				if (read_block(cur->blocks[i],tmp)==-1) // lecture du bloc a copier 	
				//allouer nouveau si introuvable	
				{
					cur->blocks[i]=get_block();
				}
				memcpy(tmp , &buf[len] ,rdnb); // memcpy copie du buf a partir de len  dans buf rdnb bytes
				
				len+=rdnb;
				write_block(cur->blocks[i],tmp);
			}
		}
		else
		{ // lecture des blocs direct et indirects 

			// lecture des bloc directs avants
			for (i=0;i<10;i++)
			//il y'a que 10 blocs directs
			{
				if (read_block (cur->blocks[i],tmp)==-1)
				{
					cur->blocks[i]=get_block();
				}
				memcpy(tmp,&buf[len],BLOCKSIZE);
				len+=BLOCKSIZE;
				write_block(cur->blocks[i],tmp);
				
			}
			nbind = ((blk -10) / 256) + 1;
			//chaque bloc indirect contient 256 blocs directs 
			// on a deja parcouru 10 blocs
			for (i=0;i<nbind-1;i++)
			{
				for (j=0;j<256;j++)
				{
					if (read_block ((cur->ind_blocks[i]).blocks[j],tmp)==-1)
					//lire les blocs indirects
					{
						(cur->ind_blocks[i]).blocks[j]=get_block();
					}
					memcpy(tmp,&buf[len],BLOCKSIZE);
					len+=BLOCKSIZE;
					write_block((cur->ind_blocks[i]).blocks[j],tmp);
				}
			}
			lstind = (blk-10) % 256;
			//nombre de dernier blocs idirects a parcourir
			if (lstind == 0) 
				lstind = 256;
			for (j=0;j<lstind;j++)
			{
				if (i<lstind-1) 
				{
					rdnb=BLOCKSIZE;
				}
				else
				{
					if (lstblk == 0)
						// si le reste de la division par 1024 (BLOCKSIZE) est > 0
						// on met ce rest rdnb (pour ne pas lire plus qu'a demandé l'utilisateur)
						rdnb = BLOCKSIZE;
					else
						rdnb = lstblk;
				}
				if (read_block((cur->ind_blocks[i]).blocks[j],tmp)!=-1) // lecture du bloc a copier 		
				{
					(cur->ind_blocks[i]).blocks[j]=get_block();	
				}
					memcpy(tmp , &buf[len] ,rdnb); // memcpy copie du bloc dans buf 
					// on decale le pointeur de buf pour ne pas ecraser l'ancienne lecture
					len+=rdnb;
					write_block((cur->ind_blocks[i]).blocks[j],tmp);
			}
		}	
		write_inode(fd[fdd],cur);
	}

}
int main(int argc, char const *argv[])
{
	
	//char msg[500]="Arrivés à ce point, vous savez rédiger un document, à partir d'une feuille vide ou en utilisant un modèle, l'enregistrer au format adéquat, faire des sélections, recherches et remplacements. Nous allons maintenant nous intéresser à la mise en forme des éléments textuels";
	//char rc[500]="";
	format_disk();
	mycreat("/a");
	//mycreat("/abc");
	//mycreat("/bcc");
	//mycreat("/a");
	struct inode *cur = (struct inode *) malloc(sizeof(struct inode));
	
	cur=find("/bb");
	printf("nom fichier %s numero inode : %d \n", cur->name, cur->num); 
	cur=find("/a");
	printf("nom fichier %s numero inode : %d \n", cur->name, cur->num); 
	close(f);
	return 0;
}