from flask import Flask, request
from flask_restful import Resource, Api

app = Flask(__name__)
api = Api(app)

todos = {}

# class HelloWorld(Resource):
#     def get(self):
#         return {'hello': 'world'}

class TodoSimple(Resource):
   def get(self, todo_id):
       return {todo_id: todos[todo_id]}

   def put(self, todo_id):
       ph = request.json['ph']
       ultr=request.json['ultr']
       flux=request.json['flux']
       print(" PEGOU DO PUT:::::::::::::::::::::::::::::::::::::::::;\n")
       print(ph)   
       #return {todo_id: todos[ph]}

api.add_resource(TodoSimple, '/<string:todo_id>')
# api.add_resource(HelloWorld, '/')

if __name__ == '__main__':
    app.run(host='0.0.0.0',debug=True)
    #app.run(debug=True)

