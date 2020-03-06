
## Why

The error table shows an overview of all asguard errors. 
Errors thrown by the implemented version of asguard must be linkable to 
the theoretical world. 
By providing these error codes, 
we can discuss how errors should be handled in the theoretical world,
and implement the behaviour according to the plan.

## Error Code Generation

The error code generation is just a tool for me to structure error codes. 
This is not a strict guide on how to define error codes.


The error codes consist of 3 numbers.
 
```
X.Y.Z

X = reference to the role of the error throwing entity (Leader, Follower, Candidate, None)
Y = reference to the causing caller (e.g. append_entries, alive) 
Z = increasing number (unique in the X.Y. context), starting at 0 
```

### Reference Table for causing role
| X | Description |
|:---:|:---:|
| 1 | Leader | 
| 2 | Follower | 
| 3 | Candidate | 
| 4 | None |

### Reference Table for causing caller
| Y | Description |
|:---:|:---:|
| 1 | alive | 
| 2 | append_entries | 
| 3 | reply_append | 
| 4 | nomination | 
| 5 | vote | 



## Error Table


| Error Code | Description |
|:----:|:----:|
| | | 