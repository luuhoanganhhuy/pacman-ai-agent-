#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>


#include "ai.h"
#include "utils.h"
#include "priority_queue.h"
#include "pacman.h"

extern int total_generated;
extern int total_expanded;
extern int all_max_depth;
struct heap h;

float get_reward( node_t* n );

/**
 * Function called by pacman.c
*/
void initialize_ai(){
	heap_init(&h);
}

/**
 * function to copy a src into a dst state
*/
void copy_state(state_t* dst, state_t* src){
	//Location of Ghosts and Pacman
	memcpy( dst->Loc, src->Loc, 5*2*sizeof(int) );

    //Direction of Ghosts and Pacman
	memcpy( dst->Dir, src->Dir, 5*2*sizeof(int) );

    //Default location in case Pacman/Ghosts die
	memcpy( dst->StartingPoints, src->StartingPoints, 5*2*sizeof(int) );

    //Check for invincibility
    dst->Invincible = src->Invincible;
    
    //Number of pellets left in level
    dst->Food = src->Food;
    
    //Main level array
	memcpy( dst->Level, src->Level, 29*28*sizeof(int) );

    //What level number are we on?
    dst->LevelNumber = src->LevelNumber;
    
    //Keep track of how many points to give for eating ghosts
    dst->GhostsInARow = src->GhostsInARow;

    //How long left for invincibility
    dst->tleft = src->tleft;

    //Initial points
    dst->Points = src->Points;

    //Remiaining Lives
    dst->Lives = src->Lives;   

}

node_t* create_init_node( state_t* init_state ){
	node_t * new_n = (node_t *) malloc(sizeof(node_t));
	new_n->parent = NULL;	
	new_n->priority = 0;
	new_n->depth = 0;
	new_n->num_childs = 0;
	copy_state(&(new_n->state), init_state);
	new_n->acc_reward =  get_reward( new_n );
	return new_n;
	
}

/**
 * function to return whether a life is lost
*/
bool lost_life(node_t* n){
	if (n->parent != NULL){
		if ((n->state).Lives<(n->parent)->state.Lives){
			return true;
		}
	}
	return false;
}

float heuristic( node_t* n ){
	float h = 0;
	int i=0;
	int l =0;
	int g=0;
	
	//check if there is a change from not invincible to invincible
	if (n->parent != NULL){
		if ((n->state).Invincible && !(n->parent)->state.Invincible){
			i=10;	
		}
	}
	
	//check if there is a lost life
	if (lost_life(n)){
		l =10;
	}
	
	//check if lose a game
	if((n->state).Lives <0){
		g=100;
	}
	
	//calculate the heuristic value
	h= i-l-g;
	

	return h;
}

float get_reward ( node_t* n ){
	float reward = 0;
	
	//assume the parent's point is 0 if there is no Parent
	if ( n->parent == NULL){
		reward =heuristic(n) + (n->state).Points;
	}else{
		
		//calculate the reward
		reward = heuristic(n) + (n->state).Points - (n->parent)->state.Points;
	}
	

	float discount = pow(0.99,n->depth);
   	
	return discount * reward;
}

/**
 * Apply an action to node n and return a new node resulting from executing the action
*/

bool applyAction(node_t* n, node_t** new_node, move_t action ){

	bool changed_dir = false;

    //point to n as new node's parent
	(*new_node)->parent=n;
	
	//calculate the depth and priority
	(*new_node)->depth = n->depth +1;
	(*new_node)->priority = -(*new_node)->depth;
	
	//update the move
	(*new_node)->move = action;
	
	
	//excute the move
    changed_dir = execute_move_t( &((*new_node)->state), action );
	
	//increase the num_childs if it is a valid move
	if (changed_dir){
		n->num_childs++;
	}
	
	//calculate the accumulated reward up to the initial point
	(*new_node)->acc_reward = n->acc_reward + get_reward(*new_node);
	
	return changed_dir;

}

/**
 * return the first node of the first action
*/

node_t* propagate_back_to_first_action(node_t* n){
	int total_childs= 0;
	while ( n->depth != 1){
		total_childs+= n->num_childs;
		n = n->parent;
	}
	n->num_childs= total_childs;
	return n;
	
} 

	
/**
 * Find best action by building all possible paths up to budget
 * and back propagate using either max or avg
 */

move_t get_next_move( state_t init_state, int budget, propagation_t propagation, char* stats ){
	move_t best_action = rand() % 4;

	float best_action_score[4];
	for(unsigned i = 0; i < 4; i++)
	    best_action_score[i] = INT_MIN;

	int generated_nodes = 0;
	int expanded_nodes = 0;
	int max_depth = 0;
	
	//create a array pointer point to the explored node
	node_t** explored_node = (node_t**)malloc(sizeof(node_t*)*budget);

	//Add the initial node
	node_t* n = create_init_node( &init_state );
	//Use the max heap API provided in priority_queue.h
	heap_push(&h,n);
	
	// Dijkstra' algorithm
	while (h.count > 0 ){
		
		//pop the node out of the heap
		n = heap_delete(&h);
		
		
		//check if the expanded_node still in the budget
		if (expanded_nodes < budget){
			
			//update the value explored node
			explored_node[expanded_nodes] = n;
			expanded_nodes++;
			
			
			//check all the move posibilities
			for (int j =0; j<4; j++){
				node_t* new_n = create_init_node(&(n->state));
				
				//check if the valid move
				if (applyAction(n, &new_n, j)){
					
					
					//increase the generated node for each valid move
					generated_nodes++;
					
					
					//update the max depth
					if (new_n->depth>max_depth){
						max_depth = new_n->depth;
					}
					
					//calculate the best_action_score of the first move if it is max propagation 
					if (propagation == max){
						if ((new_n->acc_reward) > best_action_score[propagate_back_to_first_action(new_n)->move]){
							best_action_score[propagate_back_to_first_action(new_n)->move] = new_n->acc_reward;
						}
					}
					
					//calculate the best_action_score of the first move if it is avg propagation 
					if (propagation == avg){
						best_action_score[propagate_back_to_first_action(new_n)->move]
							=(new_n->acc_reward)*(propagate_back_to_first_action(new_n)->num_childs)
							/(propagate_back_to_first_action(new_n)->num_childs+1);
					}
					
					
					//push the new node to the heap
					heap_push(&h,new_n);
					
					//re arrange the heap with the priority 
					max_heapify(&new_n,h.count,h.count);
					
				
				}else{
					
					//free new_node if is not a valid move
					free(new_n);
				}				
			}
			
		}else{
			
			//free the generated node that is not explored
			emptyPQ(&h);
			free(n);
		}
	}
	
	//free the explored node
	for (int i=0; i<budget; i++) {
		free(explored_node[i]);
	}
	free(explored_node);
	
	
	
	//choose best action
	float best_score = INT_MIN;
	for (int i=0;i<4;i++){
		if (best_action_score[i]> best_score){
			best_score = best_action_score[i];
			best_action = i;
		}
	}
	
	//update the total expanded, the total generated and the max_depth
	total_expanded += expanded_nodes;
	total_generated+= generated_nodes;
	if (max_depth>all_max_depth){
		all_max_depth=max_depth;
	}
	
	
	sprintf(stats, "Max Depth: %d Expanded nodes: %d  Generated nodes: %d\n",max_depth,expanded_nodes,generated_nodes);
	
	if(best_action == left)
		sprintf(stats, "%sSelected action: Left\n",stats);
	if(best_action == right)
		sprintf(stats, "%sSelected action: Right\n",stats);
	if(best_action == up)
		sprintf(stats, "%sSelected action: Up\n",stats);
	if(best_action == down)
		sprintf(stats, "%sSelected action: Down\n",stats);
	sprintf(stats, "%sScore Left %f Right %f Up %f Down %f",stats,best_action_score[left],best_action_score[right],best_action_score[up],best_action_score[down]);
	return best_action;
}

